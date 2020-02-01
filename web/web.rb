require 'sinatra'

set :public_folder, 'pub'

get '/' do
  File.read File.join(__dir__, "pub/page.html")
end
