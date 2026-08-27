import io
from collections import Counter

import numpy as np
from PIL import Image
from ultralytics import YOLO

import config


class VehicleDetector:
    def __init__(self, model_path: str = None, conf: float = None):
        self.model = YOLO(model_path or config.MODEL_PATH, task="detect")
        self.conf = conf or config.CONF_THRESHOLD

    def detect_from_bytes(self, jpeg_bytes: bytes):
        """
        Run detection on a raw JPEG byte payload (as received over MQTT).
        Returns (vehicle_count, class_counts_dict, annotated_boxes).
        """
        image = Image.open(io.BytesIO(jpeg_bytes)).convert("RGB")
        frame = np.array(image)

        results = self.model.predict(frame, conf=self.conf, verbose=False)
        result = results[0]

        class_counts = Counter()
        boxes_out = []
        names = result.names

        for box in result.boxes:
            cls_id = int(box.cls[0])
            cls_name = names.get(cls_id, str(cls_id))
            if cls_name in config.VEHICLE_CLASSES:
                class_counts[cls_name] += 1
                xyxy = box.xyxy[0].tolist()
                boxes_out.append({"class": cls_name, "conf": float(box.conf[0]), "box": xyxy})

        vehicle_count = sum(class_counts.values())
        return vehicle_count, dict(class_counts), boxes_out
