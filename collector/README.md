# hu-probe-collector

TCP-Server für JSONL-Messstrom der Probe. Legt Session-Ordner an und bietet eine minimale Web-UI.

```bash
pip install -e .
hu-probe-collector --host 0.0.0.0 --port 9100 --web-port 8088 --sessions ../sessions
```
