import config


def decide_green_duration(vehicle_count: int) -> int:
    duration = config.BASE_GREEN_SECONDS + config.SECONDS_PER_VEHICLE * vehicle_count
    duration = max(config.MIN_GREEN_SECONDS, min(config.MAX_GREEN_SECONDS, duration))
    return round(duration)


def build_light_plan(vehicle_count: int) -> dict:
    """
    Returns a full phase plan the ESP32-C6 can execute:
      green -> yellow -> red (with buzzer active during red, for pedestrian crossing)
    """
    green_s = decide_green_duration(vehicle_count)
    return {
        "phases": [
            {"state": "green", "duration": green_s, "buzzer": False},
            {"state": "yellow", "duration": config.YELLOW_SECONDS, "buzzer": False},
            {"state": "red", "duration": config.RED_SECONDS, "buzzer": True},
        ],
        "vehicle_count": vehicle_count,
    }
