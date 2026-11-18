// The Launcher's screens for the Learn site's Launcher lessons (docs/learn/guide.json, part "launcher";
// played by wiki/src/guide/launcher.tsx). Not built on its own: tools/wiki/launcher/shots.py puts the head
// of the Launcher's own tests/ui_render.cpp (its fakes and the drawing code of src/display.cpp it extracts)
// and the boot hint's lines of src/display.cpp above this file, builds it in a copy of the Launcher and
// runs it: every screen here is drawn by the Launcher's code, with what a Cardenza shows (the community's
// featured firmware and their colour covers as of 2026-11-18, Cardenza Synth's code V6QK28, its channels).
// The two text pages the Launcher draws inside an input loop (the install chooser, Replace?) and the text
// entry are drawn here line by line as src/display.cpp and src/mykeyboard.cpp draw them on the Cardenza:
// check them when those change.
// launcher_shots <folder> <covers>: writes <folder>/<lang>/<name>.pgm for zh (简体中文) and en (English).

static std::string outFolder, coverFolder;
static std::string outLang;
static void shot(const std::string &name) {
  // The canvas is grey; colour pictures (the covers) come back as lcd.cpp sends them (ui_render's save).
  std::vector<uint16_t> panel(screen.canvas.size());
  for (size_t i = 0; i < panel.size(); ++i) {
    const uint8_t g = screen.canvas[i];
    panel[i] = uint16_t((g >> 3) << 11 | (g >> 2) << 5 | (g >> 3));
  }
  if (screen.colors_.count)
    screen.colors_.paint(screen.canvas.data(), SCREEN_WIDTH, panel.data(), SCREEN_WIDTH, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);
  screen.colors_.clear();
  const std::string path = outFolder + "/" + outLang + "/" + name + ".ppm";
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(1); }
  std::fprintf(f, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
  for (uint16_t px : panel) {
    unsigned rgb[3];
    colorRgb(px, rgb[0], rgb[1], rgb[2]);
    for (unsigned v : rgb) std::fputc(int(v), f);
  }
  std::fclose(f);
}

// The covers the community sends (c=16: 16 RGB565 colours, then a 4-bit index a pixel), from <covers>/<CODE>.bin.
static std::map<std::string, std::vector<uint8_t>> covers;
static const uint8_t *cover(const std::string &code) {
  auto found = covers.find(code);
  if (found != covers.end()) return found->second.data();
  std::ifstream file(coverFolder + "/" + code + ".bin", std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (bytes.size() != colorPictureBytes(storeCoverW, storeCoverH)) return nullptr;
  return (covers[code] = bytes).data();
}

static UiStoreItem item(const char *name, const char *author, const char *version, const char *size, bool official,
                        const std::string &code, const char *date) {
  UiStoreItem it;
  it.name = name;
  it.author = author;
  it.version = version;
  it.size = size;
  it.date = date;
  it.official = official;
  it.cover = cover(code);
  return it;
}

// The store's tools on the Cardenza: the search field (typing searches, a code too), Filter, Favorites, Updates.
static String typed;
static Option tools(UiStorePage &page, const String &text = "") {
  Option row(T("Store tools"));
  typed = text;
  page.query = &typed;
  const char *const *icons[] = {ui_icons::filter, ui_icons::star, ui_icons::update};
  for (const String &label : {T("Filter"), T("Favorites"), T("Updates")}) row.buttons.push_back({label, [] {}});
  for (int i = 0; i < 3; ++i) row.buttons[i].icon = icons[i];
  return row;
}
static void store(const UiStorePage &page, int row, int button = 0) {
  const auto rows = storeRows(page);
  storeFrame(page, rows, row, storeScrollFor(rows, row, 0), button, now);
  status();
}

// The text entry as keyboard() draws it on the Cardenza (src/mykeyboard.cpp).
static void textEntry(const String &prompt, const String &subtitle, const String &text) {
  tft->fillScreen(WHITE);
  status();
  const int x = 36, titleY = 10, fieldY = 36, fieldW = 192;
  tft->text(prompt, x, titleY, 1, 20);
  if (!subtitle.isEmpty()) tft->text(uiFitText(subtitle, 30 * 6, 1), x, 24, 1, 110);
  tft->greyRect(x, fieldY, fieldW, 39, 20);
  tft->text(text + "_", x + 9, fieldY + 14, 1, 255);
  tft->text(T("Enter saves, Esc cancels"), x, 92, 1, 60);
  status();
}

// Where to install, as uiChooseInstallTarget draws it on the Cardenza (src/display.cpp): the flash as a
// bar, the place chosen black, its name and what happens to it, one button.
struct Region { uint32_t size; String name; bool free; };
static void installTarget(const String &name, uint32_t required, const std::vector<Region> &regions, int chosen) {
  pageTitle = T("Install");
  tft->beginFrame(); tft->clearClip(); tft->fillScreen(WHITE);
  const int barY = 41, barH = 18;
  uint64_t total = 0;
  for (const auto &r : regions) total += r.size;
  std::vector<int> edges = {contentX};
  uint64_t accumulated = 0;
  for (const auto &r : regions) { accumulated += r.size; edges.push_back(contentX + accumulated * contentW / total); }
  tft->text(fitText(name, contentW, 2), contentX, 8, 2, 0);
  tft->text(TF("Needs {0}", regionSize(required)), contentX, 27, 1, 110);
  for (size_t i = 0; i < regions.size(); ++i) {
    const bool on = int(i) == chosen;
    const int x = edges[i], width = std::max(0, edges[i + 1] - x - 1);
    tft->greyRect(x, barY, width, barH, on ? 0 : regions[i].free ? 248 : 195);
    if (regions[i].free && !on)
      for (int y = barY + 1; y < barY + barH; y += 4) for (int px = x + 1; px < x + width; px += 4) tft->greyRect(px, y, 1, 1, 155);
  }
  const Region &target = regions[chosen];
  const int nameY = buttonY - 30;
  tft->text(fitText(target.free ? T("Free space") : target.name, contentW, 2), contentX, nameY, 2, 0);
  tft->text(target.free ? TF("{0} / Available", regionSize(target.size)) : TF("{0} / Will be replaced", regionSize(target.size)),
            contentX, nameY + 19, 1, 110);
  actionButton(target.free ? T("Install") : T("Replace"), contentX, contentW, true);
  status();
}

// Replace? as uiConfirmInstallReplacement draws it for one place (src/display.cpp); Back starts chosen.
static void confirmReplace(const String &name, uint32_t size, int selected) {
  pageTitle = T("Install");
  tft->beginFrame(); tft->clearClip(); tft->fillScreen(WHITE);
  const int titleY = 12, scale = 2, listY = titleY + 8 * scale + 8;
  tft->text(T("Replace?"), contentX, titleY, scale, 0);
  tft->text(fitText(name, contentW, 2), contentX, listY, 2, 0);
  tft->text(TF("{0} will be overwritten", regionSize(size)), contentX, listY + 8 * 2 + 8, 1, 110);
  actionButtons(T("Replace"), T("Back"), selected);
  status();
}

static void showHome(int card) {
  pageTitle = "";
  depth = 0;
  home = card;
  homeFrame(card, card, card, 1);
}

static void scenes() {
  motion = false;
  // The start: the logo, and with installed software the countdown's hint (board.h names FN on the Cardenza).
  bootMessage(TF("Press {0} to enter Launcher", "FN"));
  shot("boot-fn");
  bootMessage("");
  shot("boot");
  bootMessage(TF("Starting {0}", "Cardenza Synth"));
  shot("boot-starting");
  // The home cards; Account, Wi-Fi and Language say their state.
  homeAccount = T("Not bound");
  homeWifi = "Home-WiFi";
  for (int card = 0; card <= lastHome; ++card) {
    showHome(card);
    shot("home-" + std::to_string(card));
  }
  homeAccount = "alex";
  showHome(7);
  shot("home-7-bound");

  // The Language card's list.
  depth = 1;
  std::vector<Option> languages;
  int english = 0, chinese = 0;
  for (int i = 0; i < kLanguageCount; ++i) {
    const Language shown = kLanguageOrder[i];
    const String name = i18nLanguageName(shown), translated = tr(i18nLanguageKey(shown));
    languages.push_back({name, [] {}, NO_COLOR, i18nPlain(translated) == i18nPlain(name) ? String() : translated});
    if (shown == Language::SimplifiedChinese) chinese = i;
    if (shown == Language::English) english = i;
  }
  pageTitle = homeTitle(9);
  menuFrame(languages, english, 0);
  status();
  shot("language-en");
  menuFrame(languages, chinese, std::clamp(chinese - 1, 0, kLanguageCount - 4));
  status();
  shot("language");

  // The community's featured firmware (Cardenza Synth first), as the store and installed software show them.
  const auto featured = [] {
    return std::vector<UiStoreItem>{item("Cardenza Synth", "203 Systems", "0.2.0", "1.8 MB", true, "V6QK28", "2026-11-14"),
                                    item("GLIDE", "CHARL3X", "3.9", "943 KB", false, "B6S4QV", "2026-11-12"),
                                    item("MiniAcid", "urtubia", "nightly-b138e8d", "768 KB", false, "GU23EL", "2026-10-04")};
  };

  // Installed software: the store's rows; the chosen one has Launch and More on its cover.
  {
    UiStorePage software;
    software.tools = Option();
    typed = "";
    software.query = &typed;
    software.more = [](int) {};
    software.items = featured();
    software.items.resize(2);
    for (auto &it : software.items) it.launch = true;
    depth = 1;
    pageTitle = homeTitle(0);
    store(software, 1);
    shot("software");
  }

  // Wi-Fi the first time: the search, the networks, the password, connecting.
  pageTitle = homeTitle(1);
  drawBusy(T("Searching\nNearby Wi-Fi networks"), 0, 0, BusyIcon::Wifi);
  shot("wifi-search");
  std::vector<Option> networks = {{"Studio-5G", [] {}, NO_COLOR, T("Secured")}, {"Home-WiFi", [] {}, NO_COLOR, T("Secured")},
                                  {"Cafe", [] {}, NO_COLOR, T("Open")}, {T("Hidden network"), [] {}, NO_COLOR, T("A network not in the list")}};
  networks[0].signalBars = 2;
  networks[1].signalBars = 4;
  networks[2].signalBars = 1;
  pageTitle = "Wi-Fi";
  menuFrame(networks, 0, 0);
  status();
  shot("wifi-list-0");
  menuFrame(networks, 1, 0);
  status();
  shot("wifi-list");
  textEntry(T("Wi-Fi password"), "Home-WiFi", "");
  shot("wifi-password-empty");
  textEntry(T("Wi-Fi password"), "Home-WiFi", "cardenza42");
  shot("wifi-password");
  drawBusy(T("Connecting") + "\nHome-WiFi", 0, 0, BusyIcon::Wifi);
  shot("wifi-connecting");

  // Download Software: the featured firmware, the search field and the tools above them.
  {
    UiStorePage list;
    list.tools = tools(list);
    list.items = featured();
    list.paging = Option(T("Pages"));
    list.paging.buttons = {{T("Previous"), nullptr}, {T("Next"), [] {}}};
    pageTitle = T("Download") + " 1/2";
    depth = 1;
    store(list, 1);
    shot("store");
    // The tools row, Filter, Favorites and Updates chosen in turn.
    for (int button = 0; button < 3; ++button) {
      store(list, 0, button);
      shot("store-tool-" + std::to_string(button));
    }
    // A code typed on the keyboard: while the keys come, then that firmware first.
    UiStorePage code;
    code.tools = tools(code, "V6Q");
    code.items = list.items;
    code.searching = true;
    pageTitle = T("Download");
    store(code, 1);
    shot("store-typing");
    code.tools = tools(code, "V6QK28");
    code.items = {featured()[0]};
    code.searching = false;
    store(code, 1);
    shot("store-code");
  }

  // Cardenza Synth's page: its channels (the community's answer for V6QK28), then installing it.
  std::vector<Option> channels = {
      {T("Stable") + " 0.2.0", [] {}, NO_COLOR, T("Released by the author when it is ready")},
      {T("Dev") + " 0.2.0-dev.45", [] {}, NO_COLOR, T("Built automatically from the newest code; may break")},
      {T("Add favorite"), [] {}, NO_COLOR, T("Save software for later")}};
  pageTitle = "Cardenza Synth";
  menuFrame(channels, 0, 0);
  status();
  shot("channels");
  menuFrame(channels, 1, 0);
  status();
  shot("channels-dev");
  pageTitle = T("Download");
  drawBusy(T("Connecting") + "\n" + T("Firmware"), 0, 0, BusyIcon::Download);
  shot("connecting");
  // An older Cardenza Synth is installed: the place starts on it (an update replaces it); free space beside.
  const std::vector<Region> flash = {{1376256, "GLIDE", false}, {1900544, "Cardenza Synth", false}, {2490368, "", true}};
  installTarget("Cardenza Synth", 1900544, flash, 1);
  shot("target-replace");
  installTarget("Cardenza Synth", 1900544, flash, 2);
  shot("target-free");
  confirmReplace("Cardenza Synth", 1900544, 1);
  shot("replace-back");
  confirmReplace("Cardenza Synth", 1900544, 0);
  shot("replace-yes");
  pageTitle = T("Download");
  drawBusy(T("Installing") + "\nCardenza Synth", 300000, 1850768, BusyIcon::Download, T("Keep the device powered"));
  shot("installing-1");
  drawBusy(T("Installing") + "\nCardenza Synth", 1250000, 1850768, BusyIcon::Download, T("Keep the device powered"));
  shot("installing-2");
  drawBusy(T("Checking\nDownload checksum"), 0, 0, BusyIcon::Download);
  shot("checking");
  drawDialog(T("Installed"), String("Cardenza Synth\n") + T("Back restarts Launcher so you can install more."),
             T("Launch"), T("Back"), 0);
  status();
  shot("installed");
  // Updates (the store's last tool): what the community has newer for what is installed.
  pageTitle = T("Updates");
  std::vector<Option> updates = {{T("Install update"), [] {}, NO_COLOR, T("Restarts when done")},
                                 {"Cardenza Synth", [] {}, NO_COLOR, TF("Installed {0}", "0.1.0"), "0.2.0"}};
  menuFrame(updates, 0, 0);
  status();
  shot("updates");

  // Sent from the website to a bound device.
  pageTitle = "";
  drawDialog(T("Install from the community?"), String("Cardenza Synth 0.2.0\n") + T("Sent to this device from the website"),
             T("Install"), T("Cancel"), 0);
  shot("remote-confirm");
  drawBusy("Cardenza Synth\n0.2.0  " + T("From the community"), 700000, 1850768, BusyIcon::Download, T("Back cancels"));
  shot("remote-installing");

  // USB Drive and File Browser: a .bin file copied to the card, installed from it.
  depth = 1;
  pageTitle = homeTitle(4);
  uiUsbMode(false);
  shot("usb-waiting");
  uiUsbMode(true);
  shot("usb-connected");
  pageTitle = homeTitle(2);
  Option folder(T("Folder tools"));
  folder.buttons = {{T("New folder"), [] {}}, {T("Paste"), nullptr}};
  std::vector<Option> files = {folder};
  const auto file = [](const String &name, bool isFolder) {
    Option row(name, [] {}, NO_COLOR, "", isFolder ? ">" : "");
    row.action = isFolder ? T("Open") : T("Install");
    row.icon = isFolder ? ui_icons::open : ui_icons::install;
    return row;
  };
  files.push_back(file("CardenzaSynth", true));
  files.push_back(file("CardenzaSynth-0.2.0.bin", false));
  files.push_back(file("downloads", true));
  menuFrame(files, 1, 0, true, true, 0);
  status();
  shot("files-folder");
  menuFrame(files, 2, 0, true, true, 0);
  status();
  shot("files-bin");

  // The Account card: the code to type on the website at once, then bound.
  const String note = T("The account is for Wi-Fi.\nUSB install needs no account.");
  pageTitle = homeTitle(7);
  depth = 0;
  UiSignInPage signIn;
  signIn.code = "K7M2-XQ9P";
  signIn.link = "cardenza.203.io/community/activate";
  signIn.status = TF("Expires in {0}", "9:41");
  signIn.note = note;
  drawSignIn(signIn);
  shot("sign-in");
  drawDialog(T("Account bound"), TF("This device is bound to {0}.", "alex"), "", "", 0);
  shot("account-bound-notice");
  std::vector<Option> bound = {
      {"alex", nullptr, NO_COLOR, TF("ID {0}", "k7m2xq9p"), "cardenza.203.io"},
      {T("Online"), nullptr, NO_COLOR, TF("Receives firmware sent from {0}", "cardenza.203.io")},
      {T("Receive firmware"), [] {}, NO_COLOR, T("Install what is sent from the website"), T("On")},
      {T("Unbind"), [] {}, NO_COLOR, T("Remove this device from the account")}};
  pageTitle = T("Account");
  depth = 1;
  menuFrame(bound, 1, 0);
  status();
  shot("account-bound");

  // Update Launcher: a newer version (the numbers are an example), updating, without Wi-Fi, sent from the website.
  pageTitle = homeTitle(6);
  depth = 1;
  UiLauncherUpdatePage update;
  update.from = "0.3.0";
  update.to = "0.4.0";
  update.size = 1363152;
  update.published = "2026-12-01";
  update.notes = UiLauncherUpdatePage::Notes::None;
  update.action = T("Update");
  drawLauncherUpdate(update);
  shot("launcher-update");
  drawBusy(T("Updating\nLauncher") + " 0.4.0", 420000, 1363152, BusyIcon::Download, T("Keep the device powered"));
  shot("launcher-updating");
  UiLauncherUpdatePage offline;
  offline.from = "0.3.0";
  offline.heading = T("Not connected");
  offline.detail = TF("Version {0}", "0.3.0");
  offline.text = T("Connect to Wi-Fi to check for a new Launcher.");
  offline.action = T("Connect to Wi-Fi");
  drawLauncherUpdate(offline);
  shot("launcher-offline");
  drawDialog(T("Update Launcher?"), TF("Version {0}", "0.4.0") + "\n" + T("Sent to this device from the website"),
             T("Update"), T("Cancel"), 0);
  shot("launcher-remote-confirm");
  depth = 0;
}

int main(int argc, char **argv) {
  outFolder = argc > 1 ? argv[1] : ".";
  coverFolder = argc > 2 ? argv[2] : ".";
  const std::pair<Language, const char *> languages[] = {{Language::SimplifiedChinese, "zh"}, {Language::English, "en"}};
  for (const auto &[language, tag] : languages) {
    i18nSetLanguage(language);
    outLang = tag;
    scenes();
  }
  return 0;
}
