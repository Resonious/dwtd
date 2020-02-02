require 'sinatra'

set :public_folder, 'pub'

get '/' do
  File.read File.join(__dir__, "page.html")
end
