# DTCore minimum consumer compile fixture

This is a host **template**, not a production plugin or another consumer project.
Copy the template into an ignored validation directory, expose the DTCore checkout
under its `Plugins/DTCore`, and build with UE5.3:

```powershell
& '<UE5.3>/Engine/Build/BatchFiles/Build.bat' DTCoreCompatHostEditor Win64 Development '-Project=<fixture>/DTCoreCompatHost.uproject' -WaitMutex
& '<UE5.3>/Engine/Build/BatchFiles/Build.bat' DTCoreCompatHost Win64 Shipping '-Project=<fixture>/DTCoreCompatHost.uproject' -WaitMutex
```

The compile fixture uses the new int32 data-type contract and instantiates Shipping
logging macros. It does not include the project sensor classes/assets and does not
certify another actual project's Blueprint or runtime behavior.

For the 2026-10-02 run, the generated directory was `Saved/DTCoreCompatHost`.
Its plugin junction shared the checkout with the main project. Do not build its
Editor target while the main Editor or commandlet is running: both use the same
DTCore DLL. Execute builds and runtime tests sequentially.

Current-project contract tests are `MA0T10.DTCoreIntegration`. The opt-in real
Slab Broker test additionally requires `MA0T10_DTCORE_SLAB_BROKER=1` and the
external publisher to send to `topic.scenario` after the log reports
`DTCORE_SLAB_BROKER_READY`. No protected Game.ini or map needs to be edited.
