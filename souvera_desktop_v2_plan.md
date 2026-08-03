# Souvera Desktop v2.0 — JMAP-Client Design Plan

## Ziel
Umstellung des Mail-Panels von IMAP auf **reinen JMAP** (kein IMAP/SMTP). 
OIDC-Login wie Android-App. Modernes Sidebar-Layout. Notes-Integration.

## Phase 1 — JmapClient (C++ HTTP Client für JMAP)

### Neue Dateien
- `src/gui/souvera/mail/JmapClient.h` — Header
- `src/gui/souvera/mail/JmapClient.cpp` — Implementation

### JMAP Client Features
- HTTP REST zu Stalwart JMAP-Endpoint
- OIDC-Bearer-Token aus Flow2Auth-Session
- Methoden: `getMailboxes()`, `queryEmails()`, `getEmailBody()`, `sendEmail()`
- JSON-Serialization via QJsonDocument
- Error-Handling mit QNetworkReply::errorString()

---

## Phase 2 — OIDC Integration in Flow2Auth

### Änderungen
- `src/gui/creds/flow2auth.h` — OIDC-Token speichern
- `src/gui/creds/webflowcredentials.h` — JMAP-Bearer abrufbar machen

### Flow
- Login Flow v2 → App-Password (File-Sync)
- Zusätzlich: OIDC-Access-Token aus Session extrahieren → JMAP Bearer (Mail)

---

## Phase 3 — JMAP Mail Models

### Neue Dateien
- `src/gui/souvera/mail/JmapMailboxModel.h/.cpp` — Mailbox-Liste
- `src/gui/souvera/mail/JmapEmailModel.h/.cpp` — Email-Liste

### Ersetzt
- `MailAccount` → JmapClient::getCurrentAccountId()
- `MailFolderModel` → JmapMailboxModel
- `MailMessageModel` → JmapEmailModel

---

## Phase 4 — Sidebar Layout

### Änderungen
- `SouveraMainWindow` — BottomBar → LeftSidebar
- Neues Layout: QSplitter(LeftSidebar, ContentArea)

### Neue Dateien
- `src/gui/souvera/LeftSidebar.h/.cpp`

---

## Phase 5 — Notes Panel

### Neue Dateien
- `src/gui/souvera/notes/NotesPanel.h/.cpp`
- `src/gui/souvera/notes/NotesApi.h/.cpp`
- `src/gui/souvera/notes/NotesModel.h/.cpp`

### API
- Nextcloud Notes REST API
- Markdown-Editor mit Preview

---

## Phase 6 — Modern UI / Theme

### Änderungen
- `souvera.qss` — Light-Mode-Unterstützung
- Theme-Switcher in LeftSidebar/Settings
- Card-Delegates für Mail-Liste
