#!/usr/bin/env tarantool

package.preload['curl.driver'] = '../curl/driver.so'

local curl = require('curl')
local fiber = require('fiber')

http = curl.http()

http:request('localhost', {headers={h1="v1", h2="v2"}})


--fiber.create(function()
--  while true do
--    fiber.sleep(0.0)
--  end
--end)
--
--fiber.create(function()
--  while true do
--    print('>>>>>>>>>>>>>>>>>>>')
--    fiber.sleep(0.0)
--  end
--end)
