# 🚀 Development Build - Quick Start

Welcome! This project is ready for local development with Arch Linux.

---

## ⚡ 5-Minute Setup

### 1. Install Dependencies
```bash
pacman -S base-devel meson wayland-protocols
```

### 2. Build & Install
```bash
cd /home/bruno/vscode/gnome-screenshot
makepkg -si --nocheck
```

### 3. Test
```bash
gnome-screenshot --help
```

**Done!** 🎉

---

## 📖 Documentation

Choose based on your needs:

| Document | Time | Purpose |
|----------|------|---------|
| **This file** | 2 min | Quick orientation |
| **PKGBUILD_DEV_QUICK.txt** | 5 min | Common commands |
| **PKGBUILD_DEV.md** | 20 min | Comprehensive guide |
| **DEV_BUILD_SUMMARY.md** | 10 min | What changed |
| **PKGBUILD** | 2 min | Package definition |

---

## 🎯 Common Tasks

### Build & Install
```bash
makepkg -si --nocheck    # Quick (no tests)
makepkg --clean -si      # Full (with tests)
```

### Test Application
```bash
gnome-screenshot              # Launch GUI
gnome-screenshot -a           # Take screenshot
gnome-screenshot --help       # View help
```

### Check Wayland Support
```bash
ldd $(which gnome-screenshot) | grep wayland
```

### Iterate Development
```bash
# 1. Edit code
nano src/screenshot-backend-wayland.c

# 2. Rebuild
makepkg -si --nocheck

# 3. Test
gnome-screenshot

# 4. Repeat
```

### Run Tests
```bash
meson test -C build --print-errorlogs
```

---

## 📁 Project Structure

```
gnome-screenshot/
├── PKGBUILD                 ← Package definition (local dev mode)
├── DEV_README.md           ← This file
├── DEV_BUILD_SUMMARY.md    ← What changed
├── PKGBUILD_DEV.md         ← Full dev guide
├── PKGBUILD_DEV_QUICK.txt  ← Quick reference
│
├── src/                    ← Source code
│   ├── meson.build
│   ├── screenshot-backend-wayland.c
│   ├── screenshot-backend-wayland.h
│   └── ...
│
├── meson.build            ← Root meson (Wayland detection)
│
└── reports/               ← Additional documentation
    ├── IMPLEMENTATION_SUMMARY.md
    ├── CHANGES_SUMMARY.md
    └── ...
```

---

## 🔑 Key Features

✅ **Local Development** - Uses current directory as source  
✅ **Fast Iteration** - Quick rebuilds with incremental compilation  
✅ **Development Version** - `2.devel` won't conflict with releases  
✅ **Wayland Support** - Auto-detection at build time  
✅ **Multi-arch** - Supports x86_64 and aarch64  
✅ **Well Documented** - Multiple guides for different needs  

---

## 🐛 Troubleshooting

### "arch-meson: command not found"
```bash
pacman -S meson
```

### "Build fails mysteriously"
```bash
makepkg -v 2>&1 | tee build.log
less build.log
```

### "Wayland support not showing"
```bash
pacman -S wayland wayland-protocols
makepkg --clean -si
ldd $(which gnome-screenshot) | grep wayland
```

See **PKGBUILD_DEV.md** for more troubleshooting.

---

## 📚 Learning Path

**Just want to build?**
1. Read this file (2 min)
2. Run: `makepkg -si --nocheck`

**Active development?**
1. Read this file (2 min)
2. Read **PKGBUILD_DEV_QUICK.txt** (5 min)
3. Start coding!

**Comprehensive understanding?**
1. Read **DEV_BUILD_SUMMARY.md** (10 min)
2. Read **PKGBUILD_DEV.md** (20 min)
3. Reference **PKGBUILD** (2 min)

**Understanding Wayland integration?**
1. Read **IMPLEMENTATION_SUMMARY.md** (in reports/)
2. Read **PKGBUILD_DEV.md** - "Wayland integration" section

---

## 🚀 Next Steps

### For Developers
```bash
1. makepkg -si --nocheck           # Build
2. gnome-screenshot                # Test
3. Edit src/screenshot-*.c         # Modify
4. makepkg -si --nocheck           # Rebuild
5. Repeat 2-4 as needed
```

### Before Committing
```bash
makepkg --clean -si                # Full build
meson test -C build                # Run tests
GDK_BACKEND=wayland gnome-screenshot   # Test Wayland
GDK_BACKEND=x11 gnome-screenshot       # Test X11
git add -A && git commit -m "..."  # Commit
```

### Converting to Release Build
```bash
# Edit PKGBUILD
# - Change pkgrel from "2.devel" to "2"
# - Or "1" if doing a new version
makepkg --clean -si    # Test full build
```

---

## 💡 Useful Commands

| What | Command |
|------|---------|
| Quick build | `makepkg -si --nocheck` |
| Full build | `makepkg --clean -si` |
| Build only | `makepkg` |
| Skip tests | `makepkg --nocheck` |
| Verbose | `makepkg -v` |
| Force rebuild | `makepkg -f` |
| Clean artifacts | `rm -rf build pkg *.pkg.tar.zst` |
| Check Wayland | `ldd $(which gnome-screenshot) \| grep wayland` |
| View logs | `cat build/meson-logs/meson-log.txt` |
| Run tests | `meson test -C build` |
| Uninstall | `pacman -R gnome-screenshot` |

---

## 🎓 Getting Help

1. **Quick answers?** → Check **PKGBUILD_DEV_QUICK.txt**
2. **Build problems?** → See **PKGBUILD_DEV.md** troubleshooting
3. **Understanding PKGBUILD?** → Read **DEV_BUILD_SUMMARY.md**
4. **Code implementation?** → Check **reports/IMPLEMENTATION_SUMMARY.md**

---

## ✨ Features of This Setup

### Smart Wayland Detection
- Automatically detects Wayland at build time
- No manual configuration needed
- Works with or without Wayland packages
- Gracefully falls back to X11

### Development-Friendly
- Uses local source directory (no git clone needed)
- Fast incremental rebuilds
- Development marker won't conflict with official versions
- Clear version: `gnome-screenshot-41.0-2.devel`

### Well Documented
- Quick start (this file)
- Quick reference (PKGBUILD_DEV_QUICK.txt)
- Comprehensive guide (PKGBUILD_DEV.md)
- Change summary (DEV_BUILD_SUMMARY.md)
- Code overview (reports/)

---

## 📋 Verification

PKGBUILD has been verified for:
- ✅ Syntax correctness
- ✅ Arch Linux standards compliance
- ✅ Wayland integration
- ✅ Backward compatibility
- ✅ Production readiness

---

## 🏁 Ready?

```bash
cd /home/bruno/vscode/gnome-screenshot
makepkg -si --nocheck
gnome-screenshot
```

**Happy coding!** 🎉

---

**Version:** 41.0-2.devel  
**Status:** ✅ Ready for development  
**Last Updated:** 2024-02-23

For detailed information, see:
- 📄 **PKGBUILD_DEV.md** - Comprehensive guide
- 📋 **PKGBUILD_DEV_QUICK.txt** - Quick commands
- 📖 **DEV_BUILD_SUMMARY.md** - What changed

