FROM node:22-alpine
WORKDIR /app
COPY server.js giant-scale-lab.html paper.pdf package.json ./
COPY vendor ./vendor
ENV PORT=8080 HOST=0.0.0.0
EXPOSE 8080
USER node
CMD ["node", "server.js"]
