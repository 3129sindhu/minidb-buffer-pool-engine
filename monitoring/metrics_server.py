from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

METRICS_FILE = Path("/app/data/metrics.prom")

class MetricsHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/metrics":
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"Not Found")
            return

        if METRICS_FILE.exists():
            metrics = METRICS_FILE.read_text()
        else:
            metrics = "# MiniDB metrics file not found yet\n"

        self.send_response(200)
        self.send_header("Content-Type", "text/plain; version=0.0.4")
        self.end_headers()
        self.wfile.write(metrics.encode("utf-8"))

if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", 9100), MetricsHandler)
    print("MiniDB metrics server running on port 9100")
    server.serve_forever()