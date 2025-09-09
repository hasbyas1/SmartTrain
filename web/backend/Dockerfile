FROM node:20

WORKDIR /app

# Copy package.json & package-lock.json
COPY package*.json ./

# Install dependencies
RUN npm install

# Install nodemon globally
RUN npm install -g nodemon

# Copy semua file project
COPY . .

EXPOSE 5000

# Jalankan nodemon untuk hot reload
CMD ["nodemon", "index.js"]
