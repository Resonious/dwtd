%i[go_forward go_backward pursue guard jump stay].each do |action|
  define_method "#{action}!" do
    Fiber.yield(action)
  end
end

