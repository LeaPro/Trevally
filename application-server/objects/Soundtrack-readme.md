# Soundtrack Auth and Playback

## 1. Availability
1. `Soundtrack` is only built on Yocto target builds.
2. On target builds, the leaf uses the shared `net_audio` ALSA sink.
3. Only one of `Soundtrack` or `CustomChannels` should be enabled at a time.

## 2. Leaf Controls and Sensors
1. `enable` - enable or disable the Soundtrack leaf.
2. `sdkReady` - indicates the Soundtrack SDK is initialized.
3. `paired` - shows whether the account is paired.
4. `authStatus` - current auth state string.
5. `deviceId` - device identifier reported by Soundtrack.
6. `isPlaying` - current playback state.
7. `isPaused` - current paused state.
8. `volume` - playback volume.
9. `pairCode` - code used to initiate pairing.
10. `pair` - trigger pairing with the current `pairCode`.
11. `unpair` - remove the current pairing.
12. `play` - start or pause playback.
13. `next` - skip to the next track or item.
14. `playFromSourceId` - source identifier for play-from selection.
15. `playFromSourceType` - source type selector, typically `playlist` or `schedule`.
16. `playFromNow` - whether play-from should start immediately.
17. `soundzone` - current zone metadata.
18. `currentTrack` - current track metadata.
19. `library` - library metadata.
20. `troubles` - diagnostics from the SDK.
21. `lastError` - last error message.

## 3. Enable and Disable
1. Set `enable=false` to stop Soundtrack and release the shared audio backend.
2. Set `enable=true` to reinitialize the SDK.
3. If `play=true` when Soundtrack is re-enabled, playback should resume after the SDK is ready.
4. If `play=false` when Soundtrack is re-enabled, it should stay idle.

## 4. Pairing
1. Set `pairCode` obtained from https://app.soundtrack.io/accounts/QWNjb3VudCwsMWI4ZmoxNHJ5bTgv/zones
2. Trigger `pair=true`.
3. Expected result:
   1. `paired=true`
   2. `authStatus` updates to a paired state
   3. `deviceId` is populated when available

## 5. Unpairing
1. Trigger `unpair=true`.
2. Expected result:
   1. `paired=false`
   2. `authStatus` updates accordingly

## 6. Playback
1. Set `play=true` to start playback.
2. Set `play=false` to pause playback.
3. Expected result:
   1. `isPlaying` reflects active playback
   2. `isPaused` reflects paused playback
   3. `currentTrack` updates with the current item metadata

## 7. Skip And Play-From
1. Trigger `next=true` to skip to the next track.
2. Set `playFromSourceId`, `playFromSourceType`, and `playFromNow` to select a source.
3. When `playFromSourceId` is set, Soundtrack applies the requested source selection.
4. `playFromSourceType` and `playFromSourceId` must match:
   1. `playlist` requires a playlist ID.
   2. `schedule` requires a schedule ID.
5. If a playlist ID is sent with `schedule` (or vice versa), the request may be rejected or appear ignored.

## 8. Volume
1. Set `volume` to a value between `0` and `100`.
2. Expected result:
   1. Playback volume changes on the target
   2. `volume` remains in sync with the active SDK value

## 9. Error Handling
1. Any SDK or playback failure sets `lastError`.
2. Common causes:
   1. SDK lookup failure
   2. Audio callback allocation failure
   3. SDK create failure
   4. Pairing or playback request failure
3. Recovery:
   1. Toggle `enable=false` then `enable=true` to reset the SDK.
   2. If pairing was lost, repeat the pairing flow.

## 10. Operational Notes
1. Soundtrack and Custom Channels share the same `net_audio` sink on Yocto builds.
2. Only one leaf should be enabled at a time.
3. If you switch from one leaf to the other, disable the active leaf first, then enable the other leaf.
4. Keep real credentials and account data out of source comments and committed docs.

[{"id":"Q29sbGVjdGlvbiw2dVBjOHU3RlZiNEZ5NENhZUFRRVVPLDAv","name":"Neon New Wave","type":"playlist","imageUri":"https://i.soundcdn.com/p/500/500/2q1bhbvo6arzajeWtsTrQKkqn4LJSdLags9Kfkj7oT4ZXYKqoDAcdYsrNUiDEx3gZyoatfsDsVSWRiCvhcS55bZxDVXg8VuNbonWkrJR5hWkZcmGd7Lu9sEzpN"},{"id":"Q29sbGVjdGlvbiw0M0t6Y0M4UGQyMGVPMEdWeUxwS2FoLDAv","name":"Pure Hard Rock Energy","type":"playlist","imageUri":"https://i.soundcdn.com/p/500/500/2q1bhbvo6arzajeWtsTrQKkqn4LJSdKH3ADE8gRchabof7SYBHvzBjkqXKs7BK2Fgqskibqp7LzqYD4pnMpC9gjW6qCx8PutX1KA7B5yz7VjZURhh1wAfqNpod"}]