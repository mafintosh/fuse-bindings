FROM debian:latest

RUN apt update -y
RUN apt upgrade -y
RUN apt install fuse libfuse-dev pkg-config python python3 curl make build-essential -y

RUN curl -sL https://deb.nodesource.com/setup_13.x | bash -
RUN apt-get install nodejs

COPY ./ ./

RUN npm install -g node-gyp
RUN npm install --unsafe-perm

CMD ["node", "example"]