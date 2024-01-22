FROM node:20-alpine
RUN apk update && \
    apk add --no-cache python3 alpine-sdk
WORKDIR /app
COPY binding.gyp package.json /app/
COPY src /app/src/
RUN npm install --production
