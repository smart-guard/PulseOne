module.exports = {
    createClient: () => ({
      connect: jest.fn().mockResolvedValue(),
      on: jest.fn(),
      get: jest.fn().mockResolvedValue(null),
      set: jest.fn().mockResolvedValue('OK'),
      del: jest.fn().mockResolvedValue(1),
      quit: jest.fn().mockResolvedValue(),
    }),
  };
  