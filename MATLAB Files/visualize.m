function t = visualize()
t = timer;
t.StartFcn = @(~,~)startVisual;
t.TimerFcn = @(~,~)body;
t.ExecutionMode = 'fixedRate';
t.TasksToExecute = Inf;
t.Period = 120;
t.StopFcn = @stopVisual;
end
