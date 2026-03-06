# GShot Main Rule File

## General Guidelines

- You do not make changes to the code unless the user says "proceed"
- You do not add emojis in generated code/documentation
- Only create new documentation files when requested
- You do not just make assumptions, you verify
- The user can make mistakes or speculate, do not assume they are always right

## Create tarball

```
cd /tmp && rm -rf gshot-alpha2 && mkdir gshot-alpha2 && cd gshot-alpha2 && git -C /home/bruno/vscode/gnome-screenshot archive HEAD | tar -x && rm -rf bin && cd .. && tar -czf /home/bruno/vscode/gnome-screenshot/bin/gshot-alpha2.tar.gz gshot-alpha2
```

```
sha256sum /home/bruno/vscode/gnome-screenshot/bin/gshot-alpha2.tar.gz
```
