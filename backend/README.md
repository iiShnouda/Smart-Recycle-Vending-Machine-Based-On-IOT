# ReWinGo Mongo Backend

Tiny Python HTTP server that bridges the Qt kiosk app to MongoDB Atlas.
Runs locally on the Pi (or your dev machine) on port 5000.

## Why this exists

MongoDB deprecated the Atlas Data API in 2025. Instead of relying on it,
we run a 100-line Flask server that uses the **official MongoDB driver**
to talk to Atlas and exposes a simple REST API for the kiosk.

## One-time setup

```bash
cd backend
python -m venv venv
source venv/bin/activate          # Linux/Mac
# venv\Scripts\activate.bat       # Windows
pip install -r requirements.txt
cp .env.example .env
# Edit .env — paste the MongoDB connection string and pick an API key.
```

## Run

```bash
python server.py
```

You should see something like:
```
 * Running on http://127.0.0.1:5000
```

Test it with curl:
```bash
curl http://127.0.0.1:5000/health
# {"status":"ok","db":"rewingo"}
```

## Wire to the kiosk app

In `src/applicationmanager.cpp` find the Mongo configure line and set:

```cpp
m_mongo->configure(
    "http://127.0.0.1:5000",         // ← local backend, not Atlas
    "<the same API key you put in .env>",
    "",                               // dataSource not used by our backend
    "rewingo");
```

(The C++ `MongoClient` will need a tiny tweak to hit our endpoints instead
of Atlas Data API ones — see the comment block in `mongo_client.cpp`.)

## Auto-start on the Pi

Create a systemd service so the backend launches at boot:

```bash
sudo tee /etc/systemd/system/rewingo-backend.service <<EOF
[Unit]
Description=ReWinGo Mongo backend
After=network-online.target

[Service]
User=pi
WorkingDirectory=/home/pi/Recycle_Vending_Machine_LCD/backend
ExecStart=/home/pi/Recycle_Vending_Machine_LCD/backend/venv/bin/python server.py
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl enable rewingo-backend
sudo systemctl start  rewingo-backend
```

## Endpoints

| Path             | Body                                       | Returns                      |
| ---------------- | ------------------------------------------ | ---------------------------- |
| POST /find       | `{collection, filter}`                     | `{documents: [...]}`         |
| POST /findOne    | `{collection, filter}`                     | `{document: {...}}`          |
| POST /insertOne  | `{collection, document}`                   | `{insertedId: "..."}`        |
| POST /insertMany | `{collection, documents: [...]}`            | `{count: N}`                 |
| POST /updateOne  | `{collection, filter, update, upsert?}`    | `{modifiedCount, upsertedId}` |
| POST /deleteOne  | `{collection, filter}`                     | `{deletedCount}`             |
| GET  /health     | -                                          | `{status: "ok", db: "..."}`  |

All endpoints require `x-api-key: <BACKEND_API_KEY>`.
