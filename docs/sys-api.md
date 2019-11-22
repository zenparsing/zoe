## Sys API

- File system
  - Paths
    - `resolveFilePath(path)`
  - Files
    - `readTextFileSync(fileURL)`
  - Directories
    - `openDirectory(fileURL, callback)`
    - `readDirectory(handle, maxEntries, callback)`
    - `closeDirectory(handle, callback)`
- Timers
  - `startTimer(timeout, repeat, callback)`
  - `stopTimer(handle)`
- URL
  - `resolveURL(url, baseURL)`
