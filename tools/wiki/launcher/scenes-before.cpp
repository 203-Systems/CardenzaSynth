// The screens of the Launcher a person updates from (0.3.1, the stable release before 0.3.2) for the lesson
// 更新启动器 (docs/learn/guide.json "update-launcher"): its home cards (Update Launcher the seventh), its Update
// Launcher page offering 0.3.2 with 0.3.2's notes, the download, and the page without Wi-Fi. Built and run by
// tools/wiki/launcher/shots.py like scenes.cpp, against that Launcher's code (--before); the screens are named
// "old-<name>".
// launcher_shots <folder> <covers>: writes <folder>/<lang>/old-<name>.ppm for zh (简体中文) and en (English).

static std::string outFolder, coverFolder;
static std::string outLang;
static void shot(const std::string &name) {
  std::vector<uint16_t> panel(screen.canvas.size());
  for (size_t i = 0; i < panel.size(); ++i) {
    const uint8_t g = screen.canvas[i];
    panel[i] = uint16_t((g >> 3) << 11 | (g >> 2) << 5 | (g >> 3));
  }
  if (screen.colors_.count)
    screen.colors_.paint(screen.canvas.data(), SCREEN_WIDTH, panel.data(), SCREEN_WIDTH, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);
  screen.colors_.clear();
  const std::string path = outFolder + "/" + outLang + "/old-" + name + ".ppm";
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

static void scenes() {
  motion = false;
  // The home cards (0.3.1: Software Launcher, Download Software, File Browser, Remote Access, USB Drive, System
  // Settings, Update Launcher, Account, Wi-Fi, Language); the running version under Update Launcher.
  homeAccount = T("Not bound");
  homeWifi = "Home-WiFi";
  for (int card = 0; card <= lastHome; ++card) {
    pageTitle = "";
    depth = 0;
    home = card;
    homeFrame(card, card, card, 1);
    shot("home-" + std::to_string(card));
  }
  // Update Launcher: 0.3.1 → 0.3.2 with 0.3.2's notes as the community draws them (<covers>/notes-0.3.2-<lang>.bin),
  // the download, and the page without Wi-Fi.
  pageTitle = homeTitle(6);
  depth = 1;
  std::ifstream notesFile(coverFolder + "/notes-0.3.2-" + outLang + ".bin", std::ios::binary);
  static std::map<std::string, std::vector<uint8_t>> notes;
  notes[outLang] = std::vector<uint8_t>((std::istreambuf_iterator<char>(notesFile)), std::istreambuf_iterator<char>());
  const auto &picture = notes[outLang];
  UiLauncherUpdatePage update;
  update.from = "0.3.1";
  update.to = "0.3.2";
  update.size = 1370848;
  update.published = "2026-11-18";
  update.notes = picture.empty() ? UiLauncherUpdatePage::Notes::Failed : UiLauncherUpdatePage::Notes::Shown;
  update.notesImage = picture.data();
  update.notesBytes = picture.size();
  update.action = T("Update");
  drawLauncherUpdate(update);
  shot("launcher-update");
  drawBusy(T("Updating\nLauncher") + " 0.3.2", 420000, 1370848, BusyIcon::Download, T("Keep the device powered"));
  shot("launcher-updating");
  UiLauncherUpdatePage offline;
  offline.from = "0.3.1";
  offline.heading = T("Not connected");
  offline.detail = TF("Version {0}", "0.3.1");
  offline.text = T("Connect to Wi-Fi to check for a new Launcher.");
  offline.action = T("Connect to Wi-Fi");
  drawLauncherUpdate(offline);
  shot("launcher-offline");
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
