from datetime import datetime, timezone
from pymongo import MongoClient, DESCENDING
import config


class GreenFlowDB:
    def __init__(self):
        self.client = MongoClient(config.MONGO_URI)
        self.db = self.client[config.MONGO_DB]
        self.detections = self.db["detections"]
        self.light_events = self.db["light_events"]
        self._ensure_indexes()

    def _ensure_indexes(self):
        self.detections.create_index([("ts", DESCENDING)])
        self.light_events.create_index([("ts", DESCENDING)])

    @staticmethod
    def _now():
        return datetime.now(timezone.utc)

    def log_detection(self, cam_id, vehicle_count, class_counts, decision):
        doc = {
            "ts": self._now(),
            "cam_id": cam_id,
            "vehicle_count": vehicle_count,
            "class_counts": class_counts,
            "decision": decision,
        }
        return self.detections.insert_one(doc).inserted_id

    def log_light_event(self, state, duration, trigger):
        doc = {
            "ts": self._now(),
            "state": state,
            "duration": duration,
            "trigger": trigger,
        }
        return self.light_events.insert_one(doc).inserted_id

    def recent_detections(self, limit=20):
        return list(self.detections.find().sort("ts", DESCENDING).limit(limit))
