# Files HTTP/Websocket API

The Files object tracks files uploaded via HTTP in three watched directories and publishes a live directory listing through a sensor element.

Watched directories:
- /mnt/data/audio-files
- /mnt/data/image-files
- /mnt/data/misc-files

These names also match the upload categories used by the web upload flow.

The only available operation on uploaded files is 'delete'.

## Upload flow

To upload a file, a multipart http form data upload (POST) is sent to a CGI endpoint.

Form fields:
- filename: uploaded file content
- category: upload target category
- metadata_json: optional JSON metadata text

Category to destination mapping in the CGI:
- firmware-update -> /var/tmp/sftp
- audio-files -> /mnt/data/audio-files
- image-files -> /mnt/data/image-files
- misc-files -> /mnt/data/misc-files

Upload behavior:
- CGI normalizes the uploaded filename to basename only
- CGI writes the uploaded bytes to destination_dir/filename
- If metadata_json is present and valid JSON, CGI writes a sidecar file:
  destination_dir/filename.metadata.json

How this interacts with Files API:
- Files watches only audio-files, image-files, and misc-files
- Files does not watch firmware-update (/var/tmp/sftp)
- New, replaced, and deleted files in watched directories are detected via inotify and reflected in directory
- The current file entry metadata field is always an empty object; sidecar metadata files are not parsed into metadata yet (TBD - could be supplied only be client, or could contain data calculated on target system, e.g. audio file duration)

## Download access

Download of uploaded files will be enabled through symlinks under the webroot.

Planned layout:
- webroot/audio-files -> /mnt/data/audio-files
- webroot/image-files -> /mnt/data/image-files
- webroot/misc-files -> /mnt/data/misc-files

This keeps URL paths aligned with upload categories and directory keys in the Files API while allowing HTTP download via the web server.

## Exposed API

### Sensor element: directory

The object exposes one JsonSensor element named directory.

It publishes an object with this top-level shape:

    {
      "audio-files": { ...files... },
      "image-files": { ...files... },
      "misc-files": { ...files... }
    }

Each file entry is keyed by filename and contains:
- size: human-readable decimal units (B, KB, MB, GB, TB)
- timestamp: UTC ISO-8601 string (example: 2026-08-04T14:24:10Z)
- metadata: currently an empty object 

Example file entry:

    "cover.jpg": {
      "size": "845 KB",
      "timestamp": "2026-08-04T14:24:10Z",
      "metadata": {}
    }

## Custom method: delete

File deletion is implemented as a custom method on the Files object itself (not as an element).

Supported request payloads:
- A string path, for example: audio-files/tune.mp3

Return value:
- true on success
- error/exception on failure

## Delete path validation rules

The delete method only allows deletion under the three watched directories.

Validation performed:
- Path must be relative, not absolute
- No . or .. path segments
- First path segment must be one of:
  - audio-files
  - image-files
  - misc-files
- Path must include a file name
- Target must resolve inside the selected watched root
- Target must exist and be a regular file

If validation fails, the method returns an error reason.

## Update behavior

The object monitors directories via inotify

On startup:
- Performs one bootstrap scan per watched directory

During runtime:
- Rebuilds only changed directory entries when inotify events arrive
- Publishes an updated directory JSON when changes are detected

