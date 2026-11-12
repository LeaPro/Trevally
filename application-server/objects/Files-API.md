# Files HTTP/Websocket API

The Files object tracks files uploaded via HTTP in three watched directories and publishes a live directory listing through a sensor element.

Watched directories:
- /mnt/data/audio-files
- /mnt/data/image-files
- /mnt/data/misc-files

These names also match the upload categories used by the web upload flow.

The only available operation on uploaded files is 'delete'.

## Upload flow

To upload a file, a multipart http form data upload (POST) is sent to a CGI endpoint
(/http-multipart-form-data-upload.cgi).

Form fields:
- file: uploaded file content
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

Firmware upload progress:
- While a file named *.tar.xz.enc is uploading, the CGI sets /misc/fwUpdateStatus to Downloading and
  /misc/fwDownloadProgress to the percentage received (100 once the file is saved), which drives the
  screen overlay and the purple LED pulse. sftp-monitor then takes over (Decrypting, Extraction, ...)
- The decision is made from the file name, since the category field is not readable until the whole
  upload has arrived; uploads with any other file name never touch /misc
- If the upload breaks off, the CGI sets DownloadingFail; if the file turns out not to be a
  firmware-update upload, it sets None and progress back to 0

How this interacts with Files API:
- Files watches only audio-files, image-files, and misc-files
- Files does not watch firmware-update (/var/tmp/sftp)
- New, replaced, and deleted files in watched directories are detected via inotify and reflected in directory
- The file entry metadata field contains the parsed contents of the file's sidecar metadata file (empty object if there is none); sidecar files themselves are not listed as entries

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
- size: integer byte count
- timestamp: UTC ISO-8601 string (example: 2026-08-04T14:24:10Z)
- metadata: contents of the sidecar metadata file, e.g. {"fileName": "cover.jpg"} (empty object if none)

Example file entry:

    "cover.jpg": {
      "size": 845000,
      "timestamp": "2026-08-04T14:24:10Z",
      "metadata": {}
    }

## Custom method: delete

File deletion is implemented as a custom method on the Files object (/files) and is listed by
"info". The file's sidecar metadata file is deleted with it. Like file upload, delete does not
require authentication.

Example request:

    {"id":110,"url":"/files","method":"delete","params":{"path":"image-files/7bab41fde4da4625afffb5f29bd4e04e"}}

Params:
- path: "<directory>/<file>", for example audio-files/tune.mp3

Return value:
- "OK" on success
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

