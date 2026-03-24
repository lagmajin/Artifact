import os
import urllib.request
import urllib.error
import xml.etree.ElementTree as ET

ICONS_TO_FETCH = [
    "add", "remove", "layers", "library_add", "video_settings",
    "vpn_key", "lightbulb", "videocam", "camera_alt", "text_fields",
    "category", "near_me", "pan_tool_alt", "folder_open", "save_as",
    "add_circle", "remove_circle", "open_in_new", "content_copy",
    "content_paste", "delete", "settings", "play_arrow", "pause", "stop",
    "skip_next", "skip_previous", "fast_forward", "fast_rewind",
    "arrow_drop_down", "arrow_right", "check", "close", "more_vert", "more_horiz",
    "undo", "redo", "zoom_in", "zoom_out", "search", "filter_center_focus",
    "tune", "volume_up", "volume_off", "visibility", "visibility_off", "lock", "lock_open"
]

COLORS = {
    "blue": "#4FC1FF",
    "green": "#89D185",
    "neutral": "#C5C5C5",
    "orange": "#CE9178",
    "purple": "#C586C0",
    "red": "#F48771",
    "yellow": "#DCDCAA"
}

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MATERIAL_DIR = os.path.join(BASE_DIR, "Material")
MATERIAL_VS_DIR = os.path.join(BASE_DIR, "MaterialVS")

os.makedirs(MATERIAL_DIR, exist_ok=True)
for color in COLORS.keys():
    os.makedirs(os.path.join(MATERIAL_VS_DIR, color), exist_ok=True)

def download_icon(name):
    svg_path = os.path.join(MATERIAL_DIR, f"{name}.svg")
    if os.path.exists(svg_path):
        return True

    for version in range(25, 0, -1):
        url = f"https://fonts.gstatic.com/s/i/materialicons/{name}/v{version}/24px.svg"
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
            with urllib.request.urlopen(req) as response:
                svg_data = response.read().decode('utf-8')
                with open(svg_path, 'w', encoding='utf-8') as f:
                    f.write(svg_data)
                print(f"Downloaded {name} (v{version})")
                return True
        except urllib.error.HTTPError as e:
            if e.code == 404:
                continue
            else:
                print(f"Error downloading {name}: {e}")
                break
        except Exception as e:
            print(f"Error downloading {name}: {e}")
            break
    print(f"Could not find {name}")
    return False

def colorize_icon(name):
    svg_path = os.path.join(MATERIAL_DIR, f"{name}.svg")
    if not os.path.exists(svg_path):
        return

    ET.register_namespace('', "http://www.w3.org/2000/svg")
    try:
        tree = ET.parse(svg_path)
    except Exception as e:
        print(f"Failed to parse {svg_path}: {e}")
        return
        
    root = tree.getroot()

    for color_name, hex_code in COLORS.items():
        # Clone tree to modify
        import copy
        new_tree = copy.deepcopy(tree)
        new_root = new_tree.getroot()

        # Add fill to all paths that are not fill="none"
        for elem in new_root.iter():
            if elem.tag.endswith('path') or elem.tag.endswith('circle') or elem.tag.endswith('rect') or elem.tag.endswith('polygon'):
                if elem.attrib.get('fill') != 'none':
                    elem.set('fill', hex_code)

        out_path = os.path.join(MATERIAL_VS_DIR, color_name, f"{name}.svg")
        new_tree.write(out_path, encoding='utf-8', xml_declaration=False)

def main():
    # Discover existing icons in Material dir to colorize them too
    existing = [f[:-4] for f in os.listdir(MATERIAL_DIR) if f.endswith('.svg')]
    all_icons = set(ICONS_TO_FETCH + existing)

    for icon in sorted(all_icons):
        download_icon(icon)
        colorize_icon(icon)
    
    print("Done fetching and colorizing icons!")

if __name__ == "__main__":
    main()
