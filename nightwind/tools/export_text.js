// analysis.js -> build/text.json (for the video generator, which has no JavaScript engine)
const fs = require("fs"), path = require("path");
global.window = {}; require(path.join(__dirname, "..", "analysis.js"));
fs.writeFileSync(path.join(__dirname, "..", "build", "text.json"), JSON.stringify(window.NW_TEXT));
console.log("wrote build/text.json");
