# ReWinGo — How The Code Works

A walk-through of every C++ class, every QML pattern, and the Qt
features used in this project. Read top to bottom.

---

## Table of contents

1. [Qt fundamentals refresher](#1-qt-fundamentals-refresher)
2. [Project architecture overview](#2-project-architecture-overview)
3. [The build system — CMakeLists.txt](#3-the-build-system--cmakeliststxt)
4. [main.cpp — where Qt starts](#4-maincpp--where-qt-starts)
5. [ApplicationManager — the central coordinator](#5-applicationmanager--the-central-coordinator)
6. [TranslationManager — runtime language switching](#6-translationmanager--runtime-language-switching)
7. [Logger — rate-limited rotating logs](#7-logger--rate-limited-rotating-logs)
8. [Database — SQLite + audit pattern](#8-database--sqlite--audit-pattern)
9. [Serial_Connection — talking to the STM32](#9-serial_connection--talking-to-the-stm32)
10. [ReedMonitor — Linux GPIO polling](#10-reedmonitor--linux-gpio-polling)
11. [AdminAuth — face-gated unlock state machine](#11-adminauth--face-gated-unlock-state-machine)
12. [ProductsModel — QAbstractListModel pattern](#12-productsmodel--qabstractlistmodel-pattern)
13. [DiagnosticsRunner — fire-and-collate test commands](#13-diagnosticsrunner--fire-and-collate-test-commands)
14. [YoloRunner — ONNX inference wrapper](#14-yolorunner--onnx-inference-wrapper)
15. [FaceService — enroll + identify orchestration](#15-faceservice--enroll--identify-orchestration)
16. [SupabaseClient — REST against the cloud DB](#16-supabaseclient--rest-against-the-cloud-db)
17. [Analytics — KPI computation](#17-analytics--kpi-computation)
18. [LogsViewer — admin's log-tail tool](#18-logsviewer--admins-log-tail-tool)
19. [QML — the patterns we use everywhere](#19-qml--the-patterns-we-use-everywhere)
20. [The QML pages — what each one does](#20-the-qml-pages--what-each-one-does)
21. [Threading — what runs where](#21-threading--what-runs-where)
22. [Data flow — Pi ↔ STM32 round trip](#22-data-flow--pi--stm32-round-trip)

---

## 1. Qt fundamentals refresher

### The meta-object system

Every class derived from `QObject` and marked with the `Q_OBJECT` macro
is processed by **moc** (Meta-Object Compiler) before compilation. moc
generates a hidden `.moc` file with helper code that powers:

- **signals/slots** — type-safe runtime callbacks
- **properties** — `Q_PROPERTY(type name READ x WRITE y NOTIFY z)`
- **introspection** — `obj->metaObject()->className()`
- **dynamic invocation** — `QMetaObject::invokeMethod`

Without `Q_OBJECT`, none of these work. Every header that declares a
Qt class includes it inside the class body.

### Signals and slots

A **signal** is a function declaration without a body — moc generates
the body. You "emit" it; anyone connected receives.

A **slot** is a normal member function that can be connected to a
signal.

```cpp
class Foo : public QObject {
    Q_OBJECT
signals:
    void thingHappened(int value);     // signal — no body
public slots:
    void onThing(int value);           // slot
};

// Wire them up
connect(sender, &Foo::thingHappened,
        receiver, &Bar::onThing);

// Fire
emit thingHappened(42);
```

Slots can also be regular functions (no `public slots:` block needed
since Qt 5) or **lambdas**:

```cpp
connect(button, &QPushButton::clicked,
        this, [this]() { qDebug() << "clicked"; });
```

### Properties

A property is a single field that has a getter, an optional setter,
and a "changed" notifier signal. QML can bind to it.

```cpp
Q_PROPERTY(int points READ points WRITE setPoints NOTIFY pointsChanged)

public:
    int points() const { return m_points; }
    void setPoints(int p) {
        if (m_points == p) return;
        m_points = p;
        emit pointsChanged();
    }
signals:
    void pointsChanged();
```

In QML, `myObject.points` reads it. Setting it triggers `setPoints`
which emits the notifier — any QML binding that uses `points`
re-evaluates automatically.

### QML singletons

`QML_NAMED_ELEMENT(Foo) + QML_SINGLETON` exposes a C++ class as a
**global QML object**. Use it for things every page needs to talk to.

```cpp
class AppManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppManager)
    QML_SINGLETON
public:
    // QML asks "give me the instance" — we hand back a pointer.
    static AppManager *create(QQmlEngine *, QJSEngine *) {
        return s_instance;
    }
    static AppManager *s_instance;
};
```

In `main.cpp`:
```cpp
AppManager app;
AppManager::s_instance = &app;
```

In QML:
```qml
import Recycle_Vending_Machine_LCD
Component.onCompleted: AppManager.doStuff()
```

### Q_INVOKABLE

A regular C++ member function can be marked `Q_INVOKABLE` to make it
callable from QML even if it's not a slot.

```cpp
Q_INVOKABLE void rotateStepper(int turns, int dir);
```

QML: `AppManager.rotateStepper(2, 0)`.

---

## 2. Project architecture overview

```
┌────────────────────────────────────────────────────────────────┐
│                            QML                                  │
│   SleepMode → Language → Auth → Main → {Recycle, Vending}      │
│                       Admin gate → Admin panel                  │
│                  Consent → FaceEnroll → Done                    │
└─────────────────────────────┬──────────────────────────────────┘
                              │  via QML_SINGLETON properties &
                              │  Q_INVOKABLE methods
┌─────────────────────────────▼──────────────────────────────────┐
│              ApplicationManager  (the coordinator)              │
└─┬──────┬─────────┬─────────┬───────┬─────────┬────────┬────────┘
  │      │         │         │       │         │        │
  ▼      ▼         ▼         ▼       ▼         ▼        ▼
Logger  Database  Serial   Reed   Admin    Yolo    Supabase
                  (USB)    GPIO   Auth     +Face   (cloud)
                  worker
                  thread
```

**Single responsibility per class.** Each one does exactly one thing
and exposes its API through signals/slots.

---

## 3. The build system — CMakeLists.txt

Top-down read of `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(Recycle_Vending_Machine_LCD VERSION 0.1 LANGUAGES CXX)
```
Minimum CMake version + project name + C++ language.

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)         # auto-run moc on Q_OBJECT files
set(CMAKE_AUTORCC ON)         # auto-compile .qrc resources
```
Enable C++17 and Qt's auto-tools.

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Qml SerialPort
    Sql Network Multimedia MultimediaQuick)
```
Required Qt modules. Each enables specific classes (e.g. `Qt6::Sql`
gives `QSqlDatabase`).

```cmake
option(USE_OPENCV "Enable OpenCV for YOLO" OFF)
if (USE_OPENCV)
    find_package(OpenCV REQUIRED COMPONENTS dnn imgproc imgcodecs)
    add_compile_definitions(HAVE_OPENCV)
endif()
```
OpenCV is **optional**. Without it, `YoloRunner` returns mock
detections so the rest still works.

```cmake
qt_add_executable(appRecycle_Vending_Machine_LCD src/main.cpp)
qt_add_qml_module(appRecycle_Vending_Machine_LCD
    URI Recycle_Vending_Machine_LCD
    VERSION 1.0
    QML_FILES ...
    SOURCES ...
    RESOURCES ...)
```
`qt_add_qml_module` is the modern way to register a QML module. It:
- Creates the URI (`import Recycle_Vending_Machine_LCD` in QML)
- Compiles QML files into the binary
- Registers types declared with `QML_ELEMENT` / `QML_SINGLETON`
- Bundles resources

`target_link_libraries` then adds the Qt libs your code needs.

---

## 4. main.cpp — where Qt starts

Real bootstrap is short — `main` constructs the engine and lets the
QML scene drive everything afterward.

```cpp
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);            // event loop owner
    QQuickStyle::setStyle("Material");          // QML controls style

    ApplicationManager appManager;
    appManager.initialize();                    // wires up everything

    QQmlApplicationEngine engine;
    appManager.setQmlEngine(&engine);
    engine.rootContext()->setContextProperty("appManager", &appManager);

    engine.load(QUrl("qrc:/.../Main.qml"));

    return app.exec();                          // event loop runs forever
}
```

`appManager` is **also** registered as a QML singleton via
`QML_NAMED_ELEMENT(AppManager)`. Both `appManager.foo()` (context
property) and `AppManager.foo()` (singleton import) reach the same C++
instance.

---

## 5. ApplicationManager — the central coordinator

`include/applicationmanager.h` + `src/applicationmanager.cpp`.

### Responsibility
- **Construct** every other backend class.
- **Wire** their signals/slots.
- **Bridge** QML calls to the cross-thread serial worker.
- **Re-emit** signals so QML can react to backend events.

### Key pieces

```cpp
ApplicationManager *s_instance;          // singleton accessor for QML
QThread           *m_serialThread;       // dedicated thread
Serial_Connection *m_serial;             // lives ON m_serialThread
ReedMonitor       *m_reed;               // on GUI thread
Database          *m_database;
YoloRunner        *m_faceYolo, *m_recycleYolo;
SupabaseClient    *m_supabase;
```

### initialize() walkthrough

1. **Translation manager** — set up + load English.
2. **Serial worker thread**:
   ```cpp
   m_serialThread = new QThread(this);
   m_serial       = new Serial_Connection();   // no parent
   m_serial->moveToThread(m_serialThread);
   ```
   `moveToThread` changes the object's "thread affinity". From now on,
   any slot invoked on `m_serial` runs on `m_serialThread`. Signals
   it emits get queued for the receiver's thread automatically.

3. **Connect worker signals → our handlers** (queued):
   ```cpp
   connect(m_serial, &Serial_Connection::connected,
           this,     &ApplicationManager::onSerialConnected);
   ```
   Qt detects the threads are different → uses `QueuedConnection`
   → handler runs on GUI thread (safe to update QML).

4. **Cross-thread method call**:
   ```cpp
   QMetaObject::invokeMethod(m_serial, "start", Qt::QueuedConnection,
                             Q_ARG(QString, "AUTO"),
                             Q_ARG(int,     115200));
   ```
   Posts an event to `m_serial`'s event loop. The worker thread picks
   it up and calls `start("AUTO", 115200)` from inside its own thread.

5. **Logger** + **Database** + **ReedMonitor** + **YoloRunner** +
   **SupabaseClient** — each set up in their own section, with the
   logger's `auditEvent` signal funneled into the database.

### Destructor — clean thread shutdown

```cpp
QMetaObject::invokeMethod(m_serial, "stop", Qt::BlockingQueuedConnection);
m_serialThread->quit();
m_serialThread->wait(2000);
```

`BlockingQueuedConnection` is queued but the caller blocks until the
slot finishes — guarantees the port is closed before we proceed.

`quit()` asks the thread's event loop to exit. `wait()` blocks the
GUI thread until the thread actually finishes. Without this you'd see
"QThread destroyed while still running" warnings.

---

## 6. TranslationManager — runtime language switching

Loads a `.qm` compiled translation file, installs a `QTranslator`
into the `QCoreApplication`. When swapped, all `qsTr("…")` calls
return the new translation.

The trick: QML doesn't auto-retranslate. You bind every translated
text to a "tick" property the manager bumps on language change:

```qml
property int langTick: 0
Connections { target: appManager
    function onLanguageChanged() { langTick++ } }
Text { text: { langTick; return qsTr("Hello") } }
```

`langTick` inside the binding forces re-evaluation when it changes.

---

## 7. Logger — rate-limited rotating logs

`include/logger.h` + `src/logger.cpp`.

### Why
- Console `qDebug` is fine in development.
- Production needs a **file** for forensics.
- The `Serial_Connection` retry loop can spam "reconnecting..." 100
  times a second — naive logging would chew through the disk.
- Audit events (transactions, admin actions) need separate handling
  (uploaded to cloud).

### Pieces

```cpp
class Logger : public QObject {
public:
    enum Level { INFO, WARN, ERROR, AUDIT };

    static Logger *instance();          // classic singleton

    static void info(const QString &cat, const QString &msg,
                     const QVariantMap &meta = {});
    static void warn(...);
    static void error(...);
    static void audit(...);

signals:
    void auditEvent(...);               // DB layer subscribes
};
```

### Rate limiting

```cpp
struct LastSeen { qint64 atMs; int count; };
QHash<QString, LastSeen> m_lastSeen;
```

For each `(category, msg)` pair we remember the last time we logged
it. If the same message fires again within `kRateWindowMs = 2000`,
we **suppress** it and increment a counter. When the window expires,
we emit one summary line: `"...connecting (×42 in last 2000 ms)"`.

### Rotation

Each log file is capped at 1 MB. When full:
- `rewingo.log` → `rewingo.log.1`
- `rewingo.log.1` → `rewingo.log.2`
- … up to `rewingo.log.5`
- `rewingo.log.6` is deleted (rolls off)

### Audit channel

`audit()` calls emit `auditEvent(category, msg, meta, when)`. The
Database class connects to this signal and **persists every audit
row into a SQLite table** that the sync loop ships to Supabase.

---

## 8. Database — SQLite + audit pattern

`include/database.h` + `src/database.cpp`.

### Tables

```
users         (id, name, points, consent_at, last_seen, face_embedding, delete_after)
products      (slot, name, price_points, image_path, active, last_count, last_weight_g)
transactions  (id, ts, kind, user_id, slot, amount, meta)
audit_log     (id, ts, category, msg, meta, synced)
```

### Why SQLite?
- File-based (no daemon to run on the Pi).
- ACID transactions (won't corrupt on power loss).
- WAL mode (parallel reads while one thread writes).
- Built into Qt — `QSqlDatabase::addDatabase("QSQLITE")`.

### The audit log = the sync queue

Every `Logger::audit()` row lands in `audit_log` with `synced=0`. The
`syncTick()` timer fires every 30 s, batches up to 100 unsynced rows,
POSTs them to Supabase, and marks them `synced=1` on success.

This means the system is **eventually consistent** — even with no
internet, all data is on disk; sync catches up when WiFi returns.

### Pseudonymous user_id

Transactions reference `user_id` (a UUID string), never `name`. The
name lives only in `users`. To show "Joe bought a Cola", the admin
page JOINs at view time, and every join is itself audit-logged.

This is the **GDPR-friendly pattern**.

### Key methods

```cpp
upsertProduct(slot, name, price, image, active)
setProductCount(slot, count, weightG)
recordTransaction(kind, userId, slot, amount, meta)
ensureUser(userId, name)
adjustUserPoints(userId, delta)
deleteOldTransactions(daysOlderThan)     // retention
```

---

## 9. Serial_Connection — talking to the STM32

`src/Serial_Connection.h` + `.cpp`.

### Lives on its own thread

A serial port can block (waiting for bytes, OS write buffer full).
Running it on the GUI thread would freeze the UI. So we move it.

### Reliability features

| Feature              | How                                                  |
| -------------------- | ---------------------------------------------------- |
| Auto-detect device   | Walk `QSerialPortInfo::availablePorts()` matching VID:PID `0x0483:0x5740` |
| Auto-reconnect       | `m_reconnectTimer` retries every 2 s when port is gone |
| Per-command ACK      | `m_ackTimer` waits up to `timeoutMs` for `Done`/`Error` line |
| Retry                | Up to 3 retransmissions of an un-acked command       |
| Watchdog             | Every 5 s send `PING`; after 3 missed → tear down + reconnect |

### Send queue

Commands are not sent immediately — they're enqueued. Only one is
in flight at a time. When the STM32 replies, the next one is sent.
This serializes the command stream so we never confuse which reply
matches which command.

### Buffered receive

```cpp
QByteArray m_rxBuffer;
void onReadyRead() {
    m_rxBuffer.append(m_serial->readAll());
    while (m_rxBuffer.contains('\n')) {
        const int idx = m_rxBuffer.indexOf('\n');
        QByteArray line = m_rxBuffer.left(idx).trimmed();
        m_rxBuffer.remove(0, idx + 1);
        // process line
    }
}
```

`readyRead` fires when *some* bytes arrive — could be half a line. We
accumulate into `m_rxBuffer`, slice out complete lines (`\n`-terminated),
loop until no more complete lines, leave the rest for next time.

---

## 10. ReedMonitor — Linux GPIO polling

`include/reed_monitor.h` + `src/reed_monitor.cpp`.

### Approach

Linux exposes GPIO at `/sys/class/gpio/gpioN/value`. We open it,
read the file, check `0`/`1`. Poll at 20 Hz, debounce 200 ms before
acting on a transition.

### Why polling, not interrupts?

The Pi's `/sys/class/gpio` interface doesn't give true edge-triggered
events (kernel removed that years ago in favor of `libgpiod`). 20 Hz
polling is fine for a magnetic switch and avoids the libgpiod
dependency.

### State machine

```
Idle (armed) ─── magnet present? ──► no change
                       ▼
              magnet AWAY for ≥200 ms ──► emit adminRequested()
                                          set armed = false
              (admin button "Exit") ─────► rearm()
                                          armed = true again
```

Once admin mode is entered, closing the door back **does not**
re-cancel admin. Only the explicit Exit button re-arms.

---

## 11. AdminAuth — face-gated unlock state machine

`include/admin_auth.h` + `src/admin_auth.cpp`.

State enum exposed as a Q_PROPERTY → QML reads it directly.

```
IDLE ── tap to scan ──► SCANNING
SCANNING ── 2 s elapsed ──► verify
  match    ──► ACCEPTED ──► emit unlocked()
  no match ──► attemptsRemaining--
              if 0: LOCKED for 30 s
              else: REJECTED → tap again
LOCKED  ── 30 s ──► IDLE (attempts reset)
```

The QML `AdminGatePage` watches `state` and renders different colors
+ icons per state. The state machine is the single source of truth —
QML just reflects it.

---

## 12. ProductsModel — QAbstractListModel pattern

`include/products_model.h` + `src/products_model.cpp`.

### Why a model

QML's `ListView`, `GridView`, `Repeater` consume **models**. A model
is anything that lets the view ask:
- "How many rows?"
- "Give me row N, role X."

For 8 vending slots, we expose role names (`slot`, `name`, `price`,
`image`, `active`, `count`, `weight`) and QML reads them by name:

```qml
GridView {
    model: ProductsModel
    delegate: Text { text: name + " — " + pricePoints + " pts" }
}
```

### The three required methods

```cpp
int rowCount(const QModelIndex &) const override;
QVariant data(const QModelIndex &, int role) const override;
QHash<int, QByteArray> roleNames() const override;
```

When the data changes:
- For a new row → `beginInsertRows() ... endInsertRows()`.
- For a changed cell → `emit dataChanged(idx, idx, { roles })`.
- For a full refresh → `beginResetModel() ... endResetModel()`.

The view auto-updates.

---

## 13. DiagnosticsRunner — fire-and-collate test commands

`include/diagnostics_runner.h` + `src/diagnostics_runner.cpp`.

### Pattern

It's a thin wrapper over `appManager.sendSerial(...)` that:
1. Builds the command string (`"STEP:3:1600:0"`).
2. Fires it via `sendSerial(cmd, timeout)`.
3. Listens to `commandSucceeded`/`commandFailed` from
   `ApplicationManager`.
4. Stores the result in a `QVariantMap` keyed by command string.
5. Exposes the map via Q_PROPERTY so QML reactively renders it.

### Why this design

The admin clicks 100 different test buttons over an admin session —
all asynchronous. We don't await each one; results trickle in and the
UI reflects them as they arrive.

---

## 14. YoloRunner — ONNX inference wrapper

`include/yolo_runner.h` + `src/yolo_runner.cpp`.

### What it does

Loads a YOLO `.onnx` file + a `.names` labels file. On each call to
`detect(image)`:
1. Convert `QImage` to OpenCV `cv::Mat`.
2. Resize + normalize → `cv::dnn::blobFromImage`.
3. Forward pass through the network.
4. Post-process: filter by confidence, run NMS, map class indices to
   names.

### Why opaque pimpl

The header doesn't include `<opencv2/dnn.hpp>` — that would force
every file that includes `yolo_runner.h` to know about OpenCV.
Instead we hold a `void *m_impl` and cast inside the .cpp:

```cpp
auto *net = static_cast<cv::dnn::Net *>(m_impl);
```

This is the **pimpl idiom**. Other files compile without OpenCV
headers.

### Mock mode

When `HAVE_OPENCV` isn't defined, `detect` returns a fake detection
at the center of the frame. The whole UI flow works for testing
without OpenCV installed.

### `extractFaceEmbedding`

A face-specific helper. After detecting a face, you'd typically run
a **second** model (ArcFace or FaceNet) on the crop to get an
embedding. For now this is stubbed with a deterministic
pseudo-embedding based on pixel statistics — good enough to test the
"different images → different embeddings → no match" logic.

---

## 15. FaceService — enroll + identify orchestration

`include/face_service.h` + `src/face_service.cpp`.

### Two flows from one class

```
Idle ── startEnrollment(id,name) ──► Enrolling
        feedFrame()                  collect embeddings
        × 5 frames                 ─► average + L2-normalise
                                     save to DB blob column
                                     emit enrollSucceeded

Idle ── startIdentify() ──► Identifying
        feedFrame()         compare to all stored embeddings
                            highest score ≥ 0.6 ─► emit matched
                            else / timeout      ─► emit noMatch
```

### Why feed frames in from QML

The Pi camera is exposed by `QtMultimedia` as a video stream. QML
already has a `VideoOutput` to display it. Pulling a `QImage` out of
the latest frame at 10 Hz and pushing it into `FaceService` is the
cleanest cross-language seam.

### Storage format

```cpp
QByteArray blob;
QDataStream s(&blob, QIODevice::WriteOnly);
for (float v : emb) s << v;       // raw little-endian floats
```

We serialize the vector to a binary blob, stuff it in the `users.face_embedding`
column. On compare, we deserialize back.

In production you'd **encrypt** this blob with a device-bound key —
out of scope for this draft but the hook is obvious (just pipe `blob`
through your crypto).

---

## 16. SupabaseClient — REST against the cloud DB

`include/supabase_client.h` + `src/supabase_client.cpp`.

### Just an `QNetworkAccessManager` wrapper

Supabase is PostgREST under the hood — every table is a REST endpoint:

```
GET    /rest/v1/products?select=*&active=eq.true
POST   /rest/v1/transactions          # body = JSON
PATCH  /rest/v1/users?id=eq.abc       # body = partial JSON
DELETE /rest/v1/transactions?id=eq.5
```

Headers needed:
- `apikey: <anon_key>` — public, baked into the app
- `Authorization: Bearer <jwt>` — comes from `signInWithPassword`

After sign-in, the JWT activates **Row-Level Security** policies on
your Postgres tables so users only see their own rows.

### Async via callbacks

Each call takes a `std::function<void(bool ok, ...)>` callback.
Internally:

```cpp
QNetworkReply *r = m_net.post(req, body);
connect(r, &QNetworkReply::finished, this, [r, cb]() {
    r->deleteLater();
    cb(r->error() == QNetworkReply::NoError, ...);
});
```

The lambda captures `r` and `cb`, fires when the HTTP request
completes, parses the JSON, calls back. Non-blocking — your UI keeps
animating.

### Storage upload

`uploadFile(bucket, path, bytes, mime)` POSTs to
`/storage/v1/object/<bucket>/<path>` — used to ship product images
and (optionally) encrypted face embeddings off-device.

---

## 17. Analytics — KPI computation

`include/analytics.h` + `src/analytics.cpp`.

A handful of `SELECT COUNT(*)` queries grouped by date, kind, etc.
Results stored in member fields exposed as Q_PROPERTYs:

```cpp
Q_PROPERTY(int recyclesToday  READ recyclesToday  NOTIFY changed)
Q_PROPERTY(int vendingsToday  READ vendingsToday  NOTIFY changed)
...
Q_PROPERTY(QVariantList recent READ recent NOTIFY changed)
```

QML reads them in `KPI` cards and the recent-transactions table.
Each visit to the analytics page calls `Analytics.refresh()` which
re-runs the queries and emits `changed()` → bindings re-evaluate.

`QVariantList` holding `QVariantMap` rows is the Qt way to send a
table-like result into QML. Each entry shows up as a JS object with
named fields the QML delegate can read.

---

## 18. LogsViewer — admin's log-tail tool

`include/logs_viewer.h` + `src/logs_viewer.cpp`.

`refresh()` tails the last 200 lines of `rewingo.log`. `deleteOlderThan(days)`
purges old SQLite rows. `wipeAll()` clears everything local (DB rows + log
files) but **does not** touch already-synced cloud data.

QML colors each line by level (the line contains `[ERROR]`, `[WARN]`,
`[AUDIT]` markers from Logger's format).

---

## 19. QML — the patterns we use everywhere

### StackView

Pages slide in/out on top of each other:

```qml
StackView {
    id: mainStackView
    initialItem: SleepMode {}
    pushEnter: Transition { NumberAnimation { ... } }
}
```

Pages call `stackView.push(...)`, `stackView.pop()`,
`stackView.replace(...)` to navigate.

Each page can grab the current `StackView`:

```qml
property StackView stackView: StackView.view
```

`StackView.view` is an attached property that resolves to the
ancestor `StackView`.

### Connections

To listen to a signal from somewhere else:

```qml
Connections {
    target: appManager
    function onAdminRequested() { ... }   // slot name = "on" + signal
    function onLanguageChanged() { ... }
}
```

QML auto-generates the slot from the camel-case signal name.

### Anchors

Position children relative to parent or siblings:

```qml
Rectangle {
    anchors.top: parent.top
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.topMargin: 30
}
```

`anchors.fill: parent` is the equivalent of "all four sides match
parent."

### Property bindings

Setting a property to an expression creates a **binding**. When any
property it reads changes, the expression re-evaluates:

```qml
Text { text: "Hello " + AdminAuth.adminName }
```

When `adminName` changes (via NOTIFY signal), the text updates. No
manual wiring needed.

### TapHandler vs MouseArea

TapHandler is the modern, lighter alternative for touch + mouse:

```qml
TapHandler {
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchScreen
    onTapped: doStuff()
}
```

### Animations

```qml
SequentialAnimation on opacity {
    loops: Animation.Infinite
    NumberAnimation { to: 0.35; duration: 1400 }
    NumberAnimation { to: 1.0;  duration: 1400 }
}
```

Targets a specific property of the parent item, loops, blends two
keyframes.

### Component reuse — inline `component`

```qml
component NavTile : Item {
    property string label
    signal tapped()
    width: 200; height: 200
    ...
}

NavTile { label: "Products" }
NavTile { label: "Analytics" }
```

A mini class declared inside a QML file. Great for repeating layouts.

### QtMultimedia

Camera live feed:

```qml
CaptureSession {
    camera: Camera { active: true }
    videoOutput: videoOut
}
VideoOutput { id: videoOut }
```

Pulling individual frames as `QImage`:

```qml
const img = videoOut.videoSink.videoFrame.toImage()
```

That's how `FaceEnrollPage` ships frames to `FaceService.feedFrame()`.

---

## 20. The QML pages — what each one does

| Page                    | What                                         |
| ----------------------- | -------------------------------------------- |
| `SleepMode.qml`         | Idle splash, "PRESS TO START"                |
| `LanguageSelectionPage` | EN / AR tiles                                |
| `AuthChoicePage`        | Face / QR pick                               |
| `FaceDetectionPage`     | (user-side) live ID via `FaceService.startIdentify` |
| `QRCodePage`            | Show a QR for mobile-app login               |
| `MainPage`              | Welcome + Vending / Recycle tiles            |
| `RecycleWaitingPage`    | Two circular fill bars + IR-trigger          |
| `RecycleCountingPage`   | +/- counters per item type                   |
| `RecycleSummaryPage`    | Total points, Finish / Buy from Vending      |
| `VendingPage`           | Product grid (TODO: bind to ProductsModel)   |
| `admin/AdminGatePage`   | Animated face scan with 3-try gate           |
| `admin/AdminMainPage`   | 4-tile dashboard                             |
| `admin/AdminProductsPage` | 2×4 grid of slots, edit dialog             |
| `admin/AdminDiagnosticsPage` | Test buttons + result chips             |
| `admin/AdminAnalyticsPage` | KPIs + recent transactions table          |
| `admin/AdminLogsPage`   | Tail logs, retention buttons                 |
| `registration/ConsentPage` | Privacy summary + "I agree" / "Use QR"    |
| `registration/FaceEnrollPage` | Camera preview + progress ring         |
| `registration/RegistrationCompletePage` | Success screen              |

---

## 21. Threading — what runs where

```
GUI thread (the only thread that owns QML scene)
├── QML rendering + animations
├── Most C++ singletons (Database, Logger, AdminAuth, ...)
├── ReedMonitor poll timer
└── YOLO + FaceService (runs inference on whatever thread feeds it)

Serial worker thread
├── QSerialPort
├── m_ackTimer, m_watchdogTimer, m_reconnectTimer
└── all bytes in/out
```

Signals from Serial worker → GUI cross threads automatically (Qt
detects the `connect()` is between objects on different threads and
uses `QueuedConnection`).

QML can never directly touch the worker — always goes through
`QMetaObject::invokeMethod(..., Qt::QueuedConnection, ...)`.

---

## 22. Data flow — Pi ↔ STM32 round trip

User taps "Vend slot 3" in QML:

```
QML: vendButton.onClicked
      ↓ appManager.sendSerial("STEP:3:1600:0", 10000)
ApplicationManager::sendSerial
      ↓ QMetaObject::invokeMethod(m_serial, "sendCommand", Q_ARG..., Q_ARG...)
Serial_Connection::sendCommand          (on worker thread)
      ↓ enqueue { "STEP:3:1600:0", 10000 }
      ↓ writeRaw → STM32 USB-CDC writes
ACK timer starts (10 s window)
      ↓
                                                                  STM32
                                                                  ──────
                                                                  USB CDC RX ISR
                                                                  buffers "STEP:3:1600:0\n"
                                                                  STM32_ProcessCommand
                                                                  parse → push to stepper queue
                                                                  stepperTask: dequeue
                                                                  shift gate bits via 74HC595
                                                                  Stepper_Rotate(1600, 0)
                                                                  ── motor spins ~3 s ──
                                                                  CDC_Transmit_FS("Done STEP\r\n")
      ↓
USB CDC RX (Pi side)
QSerialPort::readyRead
Serial_Connection::onReadyRead
      ↓ buffer until '\n'
      ↓ line == "Done STEP" → starts with "Done" → match
ACK timer stops
      ↓ emit commandSucceeded("STEP:3:1600:0", "Done STEP")
ApplicationManager::onSerialCommandSucceeded
      ↓ emit serialCommandSucceeded(cmd, reply)
QML Connections { onSerialCommandSucceeded: ... }
      ↓ user sees "Dispensed!"
```

That's the whole shape, end to end.

---

## Closing thoughts

**Read this document side-by-side with the code.** Each section
corresponds to a file or pair of files. The code is short on purpose
— what matters is the architecture, not line count.

When in doubt about a Qt feature:
- **moc / Q_OBJECT**: signals, slots, properties magic
- **QML_NAMED_ELEMENT + QML_SINGLETON**: expose a C++ object globally
  to QML
- **moveToThread + QueuedConnection**: cross-thread signal/slot
- **Q_INVOKABLE**: regular method callable from QML
- **QAbstractListModel**: feed lists to QML views
- **QNetworkAccessManager**: async HTTP
- **QSqlDatabase**: SQLite (and Postgres, MySQL, etc.)
- **QtMultimedia**: cameras and video
- **qsTr + langTick pattern**: runtime language switching

If someone asks "how does X work?" — open this doc, find the
corresponding section, then jump into the code with that context in
mind. You'll be answering confidently in a day or two.
