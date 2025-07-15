const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');

const env = process.env.NODE_ENV || 'development';
const basePath = path.resolve(__dirname);
const defaultConfig = yaml.load(fs.readFileSync(`${basePath}/default.yml`, 'utf8'));
const envConfig = yaml.load(fs.readFileSync(`${basePath}/${env}.yml`, 'utf8'));

module.exports = {
  ...defaultConfig,
  ...envConfig,
};
