# ArtifactStudio AE utility pack
# Trim Comp to Content

import artifact


def main():
    result = artifact.trim_comp_to_content("selectedLayers", 0, True, True)
    print(result)


if __name__ == "__main__":
    main()
