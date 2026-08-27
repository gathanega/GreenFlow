import json
import logging
import threading

import paho.mqtt.client as mqtt
from flask import Flask, jsonify

import config
from db import GreenFlowDB
from inference import VehicleDetector
from traffic_logic import build_light_plan

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
log = logging.getLogger("greenflow")

app = Flask(__name__)
db = GreenFlowDB()
detector = VehicleDetector()

CAM_ID = "cam1"
C6_NODE_ID = "c6-node1"


# ---------------------------------------------------------------------------
# MQTT callbacks
# ---------------------------------------------------------------------------
def on_connect(client, userdata, flags, reason_code, properties=None):
    log.info("Connected to MQTT broker (rc=%s)", reason_code)
    client.subscribe(config.TOPIC_CAM_IMAGE)
    client.subscribe(config.TOPIC_CAM_STATUS)
    client.subscribe(config.TOPIC_C6_STATUS)


def on_message(client, userdata, msg):
    try:
        if msg.topic == config.TOPIC_CAM_IMAGE:
            handle_camera_frame(client, msg.payload)
        elif msg.topic in (config.TOPIC_CAM_STATUS, config.TOPIC_C6_STATUS):
            log.info("Status [%s]: %s", msg.topic, msg.payload.decode(errors="ignore"))
    except Exception:
        log.exception("Error handling message on topic %s", msg.topic)


def handle_camera_frame(client, jpeg_bytes: bytes):
    vehicle_count, class_counts, boxes = detector.detect_from_bytes(jpeg_bytes)
    plan = build_light_plan(vehicle_count)

    db.log_detection(CAM_ID, vehicle_count, class_counts, plan)
    log.info("Detected %d vehicles %s -> green=%ss",
              vehicle_count, class_counts, plan["phases"][0]["duration"])

    client.publish(config.TOPIC_LIGHT_CONTROL, json.dumps(plan), qos=1)
    for phase in plan["phases"]:
        db.log_light_event(phase["state"], phase["duration"], trigger="auto")


def start_mqtt():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="greenflow-server")
    if config.MQTT_USERNAME:
        client.username_pw_set(config.MQTT_USERNAME, config.MQTT_PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    # Large JPEG frames can exceed the default paho message size limits on some brokers;
    # ensure your broker (e.g. mosquitto.conf) sets `max_packet_size` large enough (e.g. 100000).
    client.connect(config.MQTT_BROKER, config.MQTT_PORT, keepalive=60)
    client.loop_start()
    return client


# ---------------------------------------------------------------------------
# Flask API (read-only monitoring)
# ---------------------------------------------------------------------------
@app.route("/health")
def health():
    return jsonify({"status": "ok"})

@app.route("/detections/recent")
def recent_detections():
    docs = db.recent_detections(limit=20)
    for d in docs:
        d["_id"] = str(d["_id"])
        d["ts"] = d["ts"].isoformat()
    return jsonify(docs)

if __name__ == "__main__":
    mqtt_client = start_mqtt()
    try:
        app.run(host=config.FLASK_HOST, port=config.FLASK_PORT)
    finally:
        mqtt_client.loop_stop()
