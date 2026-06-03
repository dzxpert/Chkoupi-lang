import re
import html
import os

CODE = """// Import mathematical library
jibli "math";

// Define a factorial function (return type is inferred)
dalla factorial(n: tabi3i) {
    idha (n <= 1) {
        raja3 1;
    }
    raja3 n * factorial(n - 1);
}

// String concatenation & length check
dir greeting = "Salam" + " Algeria!";
ektb("%s (length = %lld)\\n", greeting, tool(greeting));

// Array usage
dir nums = [10, 20, 30];
nums[1] = 42;
ektb("nums[1] = %lld\\n", nums[1]);

// Call the function
dir result = factorial(5);
ektb("5! = %lld\\n", result);"""

# Colors matching Kanagawa/OneDark themes
COLORS = {
    'keyword': '#E6C387',    # Gold/Yellow
    'builtin': '#7E9CD8',    # Blue
    'type': '#7CA1F5',       # Light Blue
    'bool': '#957FB8',       # Purple
    'string': '#98BB6C',     # Green
    'comment': '#72716F',    # Gray
    'number': '#D27E4E',     # Orange
    'operator': '#9CABCA',   # Muted blue-gray for operators
    'default': '#DCD7BA'     # Warm white for identifiers/punctuation
}

# Key categories
KEYWORDS = {'dir', 'dima', 'idha', 'idha_mknch', 'ab9a_dor', 'dor', 'dalla', 'raja3', 'bdl', 'khyr', 'jarb', 'ila_ghalt', 'jibli'}
BUILTINS = {'ektb', 'a9ra', 'tool'}
TYPES = {'tabi3i', '3ouchri', '5iyar', 'fargh', '7arf', 'nass', 'jadwl'}
BOOLEANS = {'sa7', 'ghalt'}
OPERATORS = {'<=', '>=', '==', '!=', '=', '+', '-', '*', '/', '%', 'w', 'wla', 'machi', '->', ':'}

def tokenize_line(line):
    # Regex matching comments, strings, identifiers/keywords, numbers, and symbols
    token_pattern = re.compile(
        r'(?P<comment>//.*)'
        r'|(?P<string>"[^"]*")'
        r'|(?P<number>\b\d+(?:\.\d+)?\b)'
        r'|(?P<ident>[a-zA-Z_][a-zA-Z0-9_]*)'
        r'|(?P<operator><=|>=|==|!=|=|\+|-|\*|/|%|->|:)'
        r'|(?P<whitespace>\s+)'
        r'|(?P<other>.)'
    )
    
    tokens = []
    pos = 0
    while pos < len(line):
        m = token_pattern.match(line, pos)
        if not m:
            break
        
        group_name = m.lastgroup
        val = m.group(group_name)
        pos = m.end()
        
        # Classify token
        color = COLORS['default']
        if group_name == 'comment':
            color = COLORS['comment']
        elif group_name == 'string':
            color = COLORS['string']
        elif group_name == 'number':
            color = COLORS['number']
        elif group_name == 'operator' or (group_name == 'ident' and val in OPERATORS):
            color = COLORS['operator']
        elif group_name == 'ident':
            if val in KEYWORDS:
                color = COLORS['keyword']
            elif val in BUILTINS:
                color = COLORS['builtin']
            elif val in TYPES:
                color = COLORS['type']
            elif val in BOOLEANS:
                color = COLORS['bool']
            else:
                color = COLORS['default']
        elif group_name == 'other' and val in OPERATORS:
            color = COLORS['operator']
            
        tokens.append((val, color, group_name == 'whitespace'))
    return tokens

def generate_svg(code_str):
    lines = code_str.split('\n')
    line_height = 22
    padding_top = 50
    padding_bottom = 20
    padding_left = 30
    
    # Calculate SVG dimensions
    width = 650
    height = padding_top + len(lines) * line_height + padding_bottom
    
    # SVG template
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    
    # Define drop shadow filter and fonts
    svg.append('''  <defs>
    <filter id="shadow" x="-5%" y="-5%" width="110%" height="110%">
      <feDropShadow dx="0" dy="8" stdDeviation="12" flood-color="#000000" flood-opacity="0.3"/>
    </filter>
  </defs>''')
    
    # Background Canvas
    svg.append(f'  <rect width="100%" height="100%" fill="#0d1117" rx="8" />') # GitHub dark style outer pad
    
    # Editor Window (with shadow)
    editor_width = width - 40
    editor_height = height - 40
    svg.append(f'  <rect x="20" y="20" width="{editor_width}" height="{editor_height}" fill="#0F141C" rx="10" filter="url(#shadow)"/>')
    
    # Window Buttons (macOS style)
    svg.append('  <circle cx="45" cy="40" r="6" fill="#FF5F56"/>')
    svg.append('  <circle cx="65" cy="40" r="6" fill="#FFBD2E"/>')
    svg.append('  <circle cx="85" cy="40" r="6" fill="#27C93F"/>')
    
    # Line Numbers & Code text
    svg.append(f'  <g font-family="Consolas, Fira Code, Monaco, \'Courier New\', Courier, monospace" font-size="14">')
    
    curr_y = padding_top + 15
    for i, line in enumerate(lines):
        line_num = i + 1
        # Draw Line Number (muted gray)
        svg.append(f'    <text x="45" y="{curr_y}" fill="#3C4048" text-anchor="end" font-size="12" select="none">{line_num}</text>')
        
        # Draw Code
        tokens = tokenize_line(line)
        svg_line = []
        svg_line.append(f'    <text x="70" y="{curr_y}" xml:space="preserve">')
        
        for val, color, is_ws in tokens:
            escaped_val = html.escape(val)
            if is_ws:
                svg_line.append(escaped_val)
            else:
                svg_line.append(f'<tspan fill="{color}">{escaped_val}</tspan>')
                
        svg_line.append('</text>')
        svg.append(''.join(svg_line))
        
        curr_y += line_height
        
    svg.append('  </g>')
    svg.append('</svg>')
    
    return '\n'.join(svg)

if __name__ == '__main__':
    os.makedirs('docs', exist_ok=True)
    svg_content = generate_svg(CODE)
    with open('docs/example_snippet.svg', 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print("Generated docs/example_snippet.svg successfully!")
