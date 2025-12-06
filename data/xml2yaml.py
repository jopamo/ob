#!/usr/bin/env python3
import sys
import yaml
import xml.etree.ElementTree as ET

def xml_to_dict(node):
    # Initialize dictionary for this node
    data = {}
    
    # Process attributes
    for key, value in node.attrib.items():
        data[key] = value
        
    # Process children
    # We need to handle repeated children (sequences) and single children (mappings)
    # But for Openbox config, repeated keys in YAML usually map to repeated elements in XML
    # except our C parser expects specific structures.
    
    # My C parser expects:
    # mapping -> attributes and children mixed
    # sequence -> repeated elements
    
    # So if we have:
    # <keybind key="A">...</keybind>
    # <keybind key="B">...</keybind>
    # 
    # We want:
    # keybind:
    #   - key: A
    #     ...
    #   - key: B
    #     ...

    children_map = {}
    
    for child in node:
        tag = child.tag
        if '}' in tag:
            tag = tag.split('}', 1)[1]
            
        child_data = xml_to_dict(child)
        
        if tag not in children_map:
            children_map[tag] = []
        
        children_map[tag].append(child_data)
        
    for tag, list_of_data in children_map.items():
        # Heuristic: if there are multiple children with same tag, it's a list.
        # BUT: some things are logically lists even if there is only one (like keybinds).
        # My C parser (obtyaml.c) handles sequences generically.
        # If it sees a sequence, it iterates.
        # If it sees a mapping, it processes it as a single child.
        
        # However, if we output a list of length 1, the YAML is:
        # tag:
        #   - item
        #
        # If we output a single item:
        # tag: item
        #
        # My C parser handles Scalar, Sequence, and Mapping.
        # If the parent expects a list (like keybinds), we should probably output a list?
        # Actually, `obtyaml.c` `process_node` handles sequences by iterating and recursing.
        # It handles mappings by processing children.
        
        # If `rc.xml` has multiple keybinds, we MUST output a list.
        # If it has one, we CAN output a list or a single item.
        # The current logic in `process_mapping` (obtyaml.c):
        # "Iterate top level keys and treat them as children of root_node" (for root)
        # "Write children":
        #   if v->type == SEQUENCE: iterate items, call process_node for each.
        #   if v->type == MAPPING: call process_node (which handles mapping).
        
        # So `obtyaml.c` handles both:
        # keybind: { ... }  ->  <keybind ... />
        # keybind: [ { ... }, { ... } ] -> <keybind ... /> <keybind ... />
        
        # So we can safely output a list if count > 1, and single item if count == 1.
        # EXCEPT for things that might be empty lists? XML doesn't really have empty lists in this context (no node = no list).
        
        if len(list_of_data) > 1:
            data[tag] = list_of_data
        else:
            # Check if we should force list for consistency?
            # For now, let's keep it simple: single item is a dict.
            # One exception: if the child data itself is complex, it's fine.
            data[tag] = list_of_data[0]

    # Process text content
    text = node.text.strip() if node.text else ""
    if text:
        if not data:
            # No attributes or children, just text -> Scalar
            return text
        else:
            # Mixed content -> use 'content' key
            data['content'] = text
            
    return data

def convert(xml_path, yaml_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()
    
    # The root node itself (openbox_config) is not part of the YAML structure's top keys usually,
    # the children of root become the top keys.
    
    yaml_data = xml_to_dict(root)
    
    # If the root had attributes (like xmlns), they end up in yaml_data.
    # We probably want to strip xmlns attributes as they are XML specific?
    # My C parser doesn't look for them, so it's fine if they are there or not, 
    # but cleaner to remove.
    keys_to_remove = [k for k in yaml_data.keys() if k.startswith('xmlns')]
    for k in keys_to_remove:
        del yaml_data[k]

    with open(yaml_path, 'w') as f:
        yaml.dump(yaml_data, f, default_flow_style=False, sort_keys=False)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.xml output.yaml")
        sys.exit(1)
        
    convert(sys.argv[1], sys.argv[2])
