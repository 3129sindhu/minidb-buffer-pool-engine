FROM gcc:13

RUN apt-get update && apt-get install -y cmake make && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN mkdir -p build

CMD ["bash"]