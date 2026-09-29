# Custom Channels Auth and Playback
  Email: customchannels@leaprofessional.com
  Password: 1Uv!wN1AA8Bm
## 1. Configure Runtime Credentials
1. Set `CUSTOMCHANNELS_API_URL` (for example `https://pro.customchannels.net`).
2. Set `CUSTOMCHANNELS_DEVELOPER_KEY`.
3. Or export environment variables before starting `application-server`:

```sh
export CUSTOMCHANNELS_DEVELOPER_KEY="<your-developer-key>"
export CUSTOMCHANNELS_PLAYER_CMD='ffmpeg -nostdin -loglevel warning -i "%URL%" -f wav - 2>/tmp/customchannels-ffmpeg.log | aplay -D net_audio 2>/tmp/customchannels-aplay.log'
./application-server
4. OR just use the defaults coded in CustomChannels.cpp
```

## 2. Register Listener
1. Set `redirectUrl` and optional `successUrl`.
2. Trigger `registerListener=true`.
3. Expected result:
   1. `status=awaiting_user_authorization`
   2. `authUrl` populated

## 3. User Authorization
1. Open `authUrl` in a browser.
2. Sign in and authorize the account.
3. Capture `auth_code` from the redirect callback or query parameters.

## 4. Exchange Auth Code For Tokens
1. Set `authCode`.
2. Trigger `exchangeAuthCode=true`.
3. Expected result:
   1. `status=authenticated`
   2. `access_token` stored internally
   3. `refresh_token` stored internally
   4. `accessExpiresAt` and `refreshExpiresAt` populated

## 5. Fetch Channels
1. Trigger `fetchChannels=true`.
2. Expected result:
   1. `channels` JSON sensor populated with channel list

## 6. Select And Play
1. Set `channelId` from `channels`.
2. Optionally set `format` (for example `mp3`) and `zone`.
3. Set `play=true`.
4. Expected result:
   1. `nowPlaying` JSON populated (`url`, `type`, `metadata`)
   2. Local player process starts with returned URL
   3. `isPlaying=true`

## 7. Next Track or Message
1. Trigger `next=true`.
2. Expected result:
   1. New `nowPlaying` payload
   2. Playback restarts with a new URL

## 8. Stop Playback
1. Trigger `stop=true`.
2. Expected result:
   1. Player process terminated
   2. `isPlaying=false`

## 9. Refresh Token
1. Trigger `refreshAccess=true`.
2. Expected result:
   1. New access and refresh token pair stored
   2. Expiry timestamps updated
   3. `status` remains `authenticated`

## 10. Error Handling
1. Any API failure sets `lastError`.
2. Common causes:
   1. Unauthorized or expired auth code
   2. Expired refresh token
   3. Invalid developer key
   4. Cancelled account
3. Recovery:
   1. If refresh fails due to expiry, rerun register -> authorize -> exchange flow.

## Operational Notes
1. `awaiting_user_authorization` after register is expected and indicates step 3 is pending.
2. Track and message URLs can expire quickly; `next` or re-`play` fetches a fresh URL.
3. Do not store real developer keys or tokens in source comments committed to git.
