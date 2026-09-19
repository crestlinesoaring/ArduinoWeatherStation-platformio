import io
import os
from datetime import datetime

from platformio.device.monitor.filters.base import DeviceMonitorFilterBase


class LogToFileEnv(DeviceMonitorFilterBase):
    NAME = "log2file_env"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._log_fp = None

    def __call__(self):
        log_dir = os.path.join(self.project_dir or ".", "logs")
        os.makedirs(log_dir, exist_ok=True)

        env = self.environment or "unknown"
        stamp = datetime.now().strftime("%y%m%d-%H%M%S")
        log_file_name = os.path.join(log_dir, f"{env}-device-monitor-{stamp}.log")

        print("--- Logging an output to %s" % os.path.abspath(log_file_name))
        # pylint: disable=consider-using-with
        self._log_fp = io.open(log_file_name, "w", encoding="utf-8")
        return self

    def __del__(self):
        if self._log_fp:
            self._log_fp.close()

    def rx(self, text):
        self._log_fp.write(text)
        self._log_fp.flush()
        return text
