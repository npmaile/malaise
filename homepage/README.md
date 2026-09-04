# homepage

The organisation's marketing site. A single static page, because a build
step would imply the content might change.

## Use

```sh
python3 -m http.server -d homepage 8000   # or just open homepage/index.html
```

## Deployment

`.github/workflows/pages.yml` uploads this directory as a GitHub Pages
artifact and deploys it on every push to `main` that touches `homepage/`, via
`actions/upload-pages-artifact` + `actions/deploy-pages`. No build step,
because there's nothing to build — `index.html` is the whole site.

One-time repo setup (not done by the workflow): in **Settings → Pages**, set
**Source** to **GitHub Actions**. After that the workflow owns deploys; the
published site lands at `https://npmaile.github.io/malaise/`.
