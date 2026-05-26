# Contributing to ReWinGo

Thanks for taking the time! This document covers the basics — code
style, branching, how to propose a change.

## Getting set up

1. Fork the repo, clone your fork:
   ```bash
   git clone https://github.com/<you>/rewingo.git
   cd rewingo
   ```
2. Follow [`INSTALL.md`](INSTALL.md) → Path B to build from source.
3. Run the test build on a Pi, or just `cmake --build build` on your
   dev machine — the QML UI runs fine without the STM32 attached
   (commands time out gracefully).

## Branching

- `main` — protected, always deployable.
- Feature branches: `feat/<short-description>` or `fix/<short-description>`.
- Tag releases as `v0.x.y` — the CI workflow attaches a `.deb` to the
  GitHub Release automatically.

## Code style

### C++

- Qt-style naming: `camelCase` for methods/properties, `PascalCase` for
  types, `m_` prefix for member variables, `s_` for statics.
- Headers in `include/`, implementations in `src/`. **No** `.cpp` files
  in `include/`, no headers in `src/`.
- Each header gets a docblock at the top explaining purpose, ownership,
  threading model. Look at `applicationmanager.h` for the format.
- `Q_INVOKABLE` for anything QML calls into. `Q_PROPERTY` for anything
  QML binds against.
- Comments explain **why**, not what. If the code is unclear, fix the
  code first.

### QML

- One page per file. File name matches the page name (`AdminFoo.qml`).
- Use property bindings rather than imperative state machines where
  possible.
- Translations: wrap every visible string in `qsTr(...)`. New strings
  show up after `lupdate` is re-run.
- Don't reach across pages — go through `appManager` or a singleton
  model. QML's `StackView` and `PropertyChanges` are not signals.

### Python (backend)

- 4-space indent, no tabs.
- Type hints on every function signature.
- Use `Flask` route decorators; keep request handlers tiny — push logic
  to helper functions that are easy to unit test.

## Commit messages

Keep the first line under 70 characters. Explain *why* in the body.

```
fix(dispense): handle TMC2209 stall mid-rotation

When DIAG fires during a move, the dispense state machine was
hanging in the `wait_for_move_end` loop because Stepper_Abort
didn't clear s_busy. Set s_busy = false in the abort path so
the polling loop exits.
```

Conventional Commit prefixes are encouraged but not required:
`feat`, `fix`, `docs`, `refactor`, `test`, `chore`.

## Pull requests

1. Open the PR against `main`.
2. Make sure CI is green — the workflow runs the .deb build and a smoke
   test of the bootstrap installer.
3. Reference any related issue in the description (`Closes #42`).
4. Squash before merging unless the history adds clarity.

## Reporting bugs

GitHub Issues, please. Include:

- What you did.
- What you expected.
- What actually happened.
- Log output if relevant (Logger writes to
  `~/.local/share/ReWinGo/ReWinGoKiosk/logs/` on the Pi).
- Kiosk version (`rewingo --version`) and Pi OS version
  (`cat /etc/os-release`).

## Security

For anything that looks like a vulnerability (credentials leaking,
remote code execution, auth bypass), do **not** open a public issue —
email the maintainers first. We'll coordinate a fix and disclosure.
