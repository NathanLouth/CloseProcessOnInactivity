# CloseProcessOnInactivity

This project consists of **two Windows programs** that work together to monitor user inactivity and automatically terminate a specified process when the system has been idle for a configured amount of time. The components are:

- `main.c` → A background process that runs under the logged-in user account. It monitors user inactivity and terminates a specified target process after a defined idle period.
- `Service.c` → A Windows service running as `LocalSystem` that ensures `main.c` is always running in the active user session.

---

## ⚠️ **Important:**
This software should **NOT** be used in production environments without comprehensive testing and a clear understanding of its behavior and security implications.  
 
Improper configuration or usage may lead to:
- **Data loss**  
  > Caused by forced termination of processes without warning or opportunity to save work.
- **Execution of unauthorized code within user sessions**  
  > If a malicious actor with local administrative privileges replaces the monitored executable, it could lead to arbitrary code being launched under another user’s context (including domain users).

You are strongly advised to **fully review and understand the source code** before deploying it in any sensitive or live environment. Proper mitigations, such as file integrity protections, code signing, and strict access controls, are your responsibility to implement.

---

## 🧩 Components

### 1. `main.c` – CloseProcessOnInactivity.exe

This executable:

- Reads configuration from the Windows registry.
- Monitors system idle time.
- If the user has been idle longer than a specified threshold, it terminates the configured process.

#### Registry Configuration

The behavior of `CloseProcessOnInactivity.exe` is controlled by registry values under:

HKEY_LOCAL_MACHINE\SOFTWARE\CloseProcessOnInactivity

| Key Name       | Type      | Description                                                                 |
|----------------|-----------|-----------------------------------------------------------------------------|
| `Run`          | `DWORD`   | If set to `1`, the program runs. If not `1`, it exits immediately.          |
| `IdleTime`     | `DWORD`   | Idle time in **seconds** before the process is terminated.                  |
| `TargetProcess`| `REG_SZ`  | Name of the executable to terminate (e.g., `notepad.exe`).                  |

#### Behavior

- Every 5 seconds, it checks system idle time.
- If the idle time exceeds the configured limit, it forcefully closes all instances of the target process.
- Runs indefinitely in a loop.

---

### 2. `Service.c` – Windows Service

This is a Windows service named:

CloseProcessOnInactivityService

Its job is to:

- Continuously check whether `CloseProcessOnInactivity.exe` is running in the active user session.
- If it is not running, it starts it.
- Ensures the monitor continues running across user logins and system reboots.

#### Hardcoded Configuration

The service is currently hardcoded to look for the executable at:

C:\Program Files\CloseProcessOnInactivity\CloseProcessOnInactivity.exe

And expects the process name to be:

CloseProcessOnInactivity.exe

You must ensure that the executable is located at that path for the service to launch it correctly.

#### Service Details

- Service name: `CloseProcessOnInactivityService`
- Interval: Checks every 10 seconds.
- Launches the monitor process in the **active console session**.

---

## 🔧 Building

Link the following libraries when building both programs:

advapi32.lib
wtsapi32.lib
userenv.lib
shlwapi.lib

Use a tool like Visual Studio or `cl.exe` with appropriate flags to build each `.c` file.

---

## 🚀 Installation Steps

1. **Compile both programs** and place `CloseProcessOnInactivity.exe` in:

    ```
    C:\Program Files\CloseProcessOnInactivity\
    ```

2. **Set registry values** under:

    ```
    HKEY_LOCAL_MACHINE\SOFTWARE\CloseProcessOnInactivity
    ```

    Example `.reg` file:
    ```reg
    Windows Registry Editor Version 5.00

    [HKEY_LOCAL_MACHINE\SOFTWARE\CloseProcessOnInactivity]
    "Run"=dword:00000001
    "IdleTime"=dword:0000003C
    "TargetProcess"="notepad.exe"
    ```

3. **Install the service**:

    Use `sc.exe` or a service installer tool:

    ```cmd
    sc create CloseProcessOnInactivityService binPath= "C:\Program Files\CloseProcessOnInactivity\CloseProcessOnInactivityService.exe"
    ```

4. **Start the service**:

    ```cmd
    sc start CloseProcessOnInactivityService
    ```

---

## 🔄 Runtime Behavior Summary

| Component | Trigger                  | Action                                                         |
|----------|---------------------------|----------------------------------------------------------------|
| `Service` | Runs at boot/login       | Ensures `CloseProcessOnInactivity.exe` runs in user session.   |
| `main`    | Monitors every 5 seconds | Closes target process if user has been idle too long.          |

---

## 📌 Notes

- Requires **Administrator privileges** to read/write `HKLM`, install services, and terminate processes.
- Ensure the target process does **not** have unsaved work, as `TerminateProcess` does not allow graceful shutdown.
- The service assumes a **single-user active session** setup.

---

## 🧼 Uninstallation

To remove the service:

```cmd
sc stop CloseProcessOnInactivityService
sc delete CloseProcessOnInactivityService
Then delete the registry key and executable files.
```

## ✅ Example Use Case
Automatically terminate applications (such as CCTV monitoring software or database viewers) if the user remains inactive for more than 2 minutes:

"Run"=dword:00000001
"IdleTime"=dword:00000078   ; (120 seconds = 2 minutes)
"TargetProcess"="cctv.exe"

## 🔐 Security Warning

This software performs operations that can pose **security and stability risks** if misused or misconfigured:

- **Runs with SYSTEM and Administrator privileges**:  
  The service launches processes and interacts with user sessions using elevated privileges. Improper use can expose the system to **privilege escalation** or unintended access.

- **Forcefully terminates processes**:  
  The `CloseProcessOnInactivity.exe` component uses `TerminateProcess`, which does **not prompt to save work** and can cause **data loss** if the target application has unsaved changes.

- **Starts processes in user sessions**:  
  The service launches the monitoring process inside active user sessions. If misconfigured or exploited, it could be used to start **unauthorized applications** with user-level privileges.

- **Executable replacement risk**:  
  The service starts a specific executable (`CloseProcessOnInactivity.exe`) from a fixed path. If an attacker with administrative access replaces that binary, they could cause the system to run arbitrary code in the context of the currently logged-in user. While this requires elevated privileges, it reinforces the need to protect the executable’s location from unauthorized modification.

### 🛡 Recommendations

- Only install this software on **trusted, controlled systems**.
- Ensure proper **ACLs (permissions)** on:
  - The executable files
  - The registry keys under `HKLM\SOFTWARE\CloseProcessOnInactivity`
- Monitor for unauthorized changes to the configuration or executable files.
- Consider code signing the binaries to prevent tampering.
- Always test in a **non-production environment** before deployment.

## 📄 License

This project is licensed under the [MIT License](https://github.com/NathanLouth/CloseProcessOnInactivity/blob/main/LICENSE).
