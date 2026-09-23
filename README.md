# Giant-Scale On-Call

An interactive, roughly two-hour course on Eric Brewer's *Lessons from Giant-Scale Services* (IEEE Internet Computing, 2001). It's built around active learning: a pre-test, predict-before-reading, closed-book explanations graded by an AI tutor, simulator labs, questions mixed in from earlier sections, a final with free recall and a design memo, and an exportable spaced-review deck.

Open the paper's PDF alongside it. The course points to exact pages.

## Run it

The AI tutor calls the DeepSeek API. Browsers block direct calls to `api.deepseek.com` (CORS), so run the included server. It serves the page and forwards AI calls to DeepSeek. It needs only Node.js 18 or newer and has no dependencies.

```bash
cp .env.example .env          # then put your key in DEEPSEEK_API_KEY
npm start                     # or: node server.js
# open http://localhost:8080
```

Or pass the settings inline:

```bash
DEEPSEEK_API_KEY=sk-... node server.js
```

Or run it with Docker:

```bash
docker build -t giant-scale .
docker run -p 8080:8080 -e DEEPSEEK_API_KEY=sk-... giant-scale
```

### Settings

| Variable | Default | Purpose |
| --- | --- | --- |
| `DEEPSEEK_API_KEY` | — | Key used for every learner. If unset, each learner pastes their own key in the page (**AI tutor** button), and the server just forwards it. |
| `ACCESS_CODE` | — | If set, learners must enter this code to use the server's key. Set it on any public host so strangers can't spend your credit. |
| `PORT` / `HOST` | `8080` / `0.0.0.0` | Listen address. Use `HOST=127.0.0.1` to keep it local. |
| `DEEPSEEK_MODEL` | `deepseek-chat` | Default model (learners can override it in the page). |
| `RATE_LIMIT` | `120` | Max AI requests per IP per hour. Responses are also capped at 1,500 tokens. |
| `DEEPSEEK_BASE_URL` | `https://api.deepseek.com` | Upstream API base (any OpenAI-compatible endpoint). |

The key stays on the server. It is never sent to the browser or written into the page. One full run of the course costs a few cents.

### Without the server

You can open `giant-scale-lab.html` directly as a file. Everything works except AI grading, which falls back to rubric-based self-grading.

## Endpoints

- `GET /`: the course
- `GET /api/status`: tells the page whether the server has a key or needs an access code
- `POST /api/chat`: DeepSeek chat-completions proxy
- `GET /healthz`: liveness check

Progress is saved in each learner's browser (localStorage).
