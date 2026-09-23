FROM node:22-alpine
WORKDIR /app
COPY server.js giant-scale-lab.html package.json ./
ENV PORT=8080 HOST=0.0.0.0
EXPOSE 8080
USER node
CMD ["node", "server.js"]
