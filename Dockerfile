FROM debian:latest

RUN apt update -y && apt upgrade -y && apt install fuse libfuse-dev pkg-config python python3 curl make build-essential -y

RUN curl -sL https://deb.nodesource.com/setup_13.x | bash -
RUN apt-get install -y nodejs

RUN mkdir app

COPY ./ ./

RUN npm install -g node-gyp
RUN npm install && node-gyp configure && node-gyp build

CMD ["npm", "test"]