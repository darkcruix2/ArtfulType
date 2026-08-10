import os

html = """<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <link rel="stylesheet" href="style.css">
    <script src="marked.min.js"></script>
    <script>
        function updateMarkdown(rawMarkdown) {
            document.getElementById('content').innerHTML = marked(rawMarkdown);
        }
        
        function getHtmlAsMarkdown() {
            return htmlToMarkdown(document.getElementById('content'));
        }
        
        function nodeToMd(node) {
          if (node.nodeType === Node.TEXT_NODE) return node.textContent.replace(/\\u200B/g, "");
          if (node.nodeType !== Node.ELEMENT_NODE) return "";
          const tag = node.tagName.toLowerCase();
          const children = () => Array.from(node.childNodes).map(nodeToMd).join("");
          switch (tag) {
            case "h1": return `# ${children().trim()}\\n\\n`;
            case "h2": return `## ${children().trim()}\\n\\n`;
            case "h3": return `### ${children().trim()}\\n\\n`;
            case "h4": return `#### ${children().trim()}\\n\\n`;
            case "h5": return `##### ${children().trim()}\\n\\n`;
            case "h6": return `###### ${children().trim()}\\n\\n`;
            case "p":  return `${children()}\\n\\n`;
            case "br": return "  \\n";
            case "strong": case "b": return `**${children()}**`;
            case "em": case "i": return `*${children()}*`;
            case "del": case "s": return `~~${children()}~~`;
            case "code": {
              if (node.parentElement && node.parentElement.tagName.toLowerCase() === "pre") return node.textContent;
              return `\`${node.textContent}\``;
            }
            case "pre": {
              const codeEl = node.querySelector("code");
              const lang = codeEl ? codeEl.className.replace(/language-/, "").trim() : "";
              return `\`\`\`${lang}\\n${(codeEl || node).textContent}\\n\`\`\`\\n\\n`;
            }
            case "a": return `[${children()}](${node.getAttribute("href") || ""})`;
            case "img": {
              const src = node.dataset.originalSrc || node.getAttribute("src") || "";
              return `![${node.getAttribute("alt") || ""}](${src})`;
            }
            case "ul": {
              const items = Array.from(node.children).map(function(li) { return "- " + liToMd(li); }).join("\\n");
              return `${items}\\n\\n`;
            }
            case "ol": {
              const items = Array.from(node.children)
                .map(function(li, i) { return (i + 1) + ". " + liToMd(li); }).join("\\n");
              return `${items}\\n\\n`;
            }
            case "li": return liToMd(node);
            case "blockquote": {
              const inner = children().trim().split("\\n").map(function(l) { return "> " + l; }).join("\\n");
              return `${inner}\\n\\n`;
            }
            case "hr": return `---\\n\\n`;
            case "table": {
              const rows = Array.from(node.querySelectorAll("tr"));
              if (!rows.length) return children();
              const headers = Array.from(rows[0].querySelectorAll("th,td")).map(function(c) { return c.textContent.trim(); });
              const body = rows.slice(1).map(function(r) {
                return Array.from(r.querySelectorAll("td,th")).map(function(c) { return c.textContent.trim(); }).join(" | ");
              }).filter(Boolean);
              return [headers.join(" | "), headers.map(function() { return "---"; }).join(" | ")].concat(body).join("\\n") + "\\n\\n";
            }
            case "input": return "";
            case "div": case "section": case "article": {
              const inner = children();
              return inner.endsWith("\\n") ? inner : inner + "\\n";
            }
            case "span": return children();
            case "script": case "style": return "";
            default: return children();
          }
        }
        function liToMd(li) {
          let text = "";
          for (var i = 0; i < li.childNodes.length; i++) {
            const child = li.childNodes[i];
            if (child.nodeType === Node.TEXT_NODE) {
              text += child.textContent.replace(/\\u200B/g, "");
            } else if (child.nodeType === Node.ELEMENT_NODE) {
              const t = child.tagName.toLowerCase();
              if (t === "input") continue;
              if (t === "ul" || t === "ol") {
                const nested = nodeToMd(child).replace(/\\s+$/, "");
                text += "\\n" + nested.split("\\n").map(function(l) { return "  " + l; }).join("\\n");
              } else {
                text += nodeToMd(child);
              }
            }
          }
          return text.trim();
        }
        function htmlToMarkdown(el) {
          return Array.from(el.childNodes)
            .map(nodeToMd).join("")
            .replace(/\\n{3,}/g, "\\n\\n").trim();
        }
    </script>
</head>
<body>
    <div id="content" contenteditable="true" spellcheck="true" style="outline:none; caret-color:var(--purple); min-height: 100vh;"></div>
</body>
</html>
"""

with open("/mnt/volume1/MyCode/workspace/ArtfulType/ArtfulType-ObjC/template.html", "w") as f:
    f.write(html)
