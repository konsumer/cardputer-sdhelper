// Mount IDBFS at /sdcard before the module starts so file writes persist
// to IndexedDB across page reloads.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function() {
  FS.mkdir('/sdcard');
  FS.mount(IDBFS, {}, '/sdcard');
  // Populate the in-memory FS from IndexedDB (true = populate from IDB)
  FS.syncfs(true, function(err) {
    if (err) console.warn('IDBFS populate error:', err);
  });
});
