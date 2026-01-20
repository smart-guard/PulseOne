const manager = require('./backend/lib/services/crossPlatformManager');

console.log('Project Root:', manager.paths.root);
console.log('Config Path:', manager.paths.config);
console.log('Data Path:', manager.paths.data);
console.log('Export Gateway Path:', manager.paths.exportGateway);
console.log('Is Development:', manager.isDevelopment);
