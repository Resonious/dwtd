%i[go_forward go_backward pursue guard jump stay].each do |action|
  define_method "#{action}!" do
    Fiber.yield(action)
  end
end

def perform!(action)
  Fiber.yield(action)
end

def _act_proc(&block)
  lambda { |*args| $player.instance_exec(*args, &block) }
end
