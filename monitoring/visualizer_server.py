from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

EVENTS_FILE = Path("/app/data/events.jsonl")
VISUALIZER_FILE = Path("/app/monitoring/visualizer.html")


class VisualizerHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/events"):
            if EVENTS_FILE.exists():
                events = EVENTS_FILE.read_text()
            else:
                events = ""

            self.send_response(200)
            self.send_header("Content-Type", "application/jsonl")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(events.encode("utf-8"))
            return

        if self.path == "/visualizer" or self.path == "/":
            if VISUALIZER_FILE.exists():
                html = VISUALIZER_FILE.read_text()
            else:
                html = "<h1>visualizer.html not found</h1>"

            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
            return

        self.send_response(404)
        self.end_headers()
        self.wfile.write(b"Not Found")


if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", 9100), VisualizerHandler)
    print("MiniDB visualizer server running on port 9100")
    server.serve_forever()
