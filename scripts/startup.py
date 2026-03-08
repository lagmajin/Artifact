import artifact


def _startup_banner():
    try:
        version = artifact.version()
    except Exception:
        version = "unknown"
    print(f"[Artifact Python] startup loaded (version={version})")


_startup_banner()
