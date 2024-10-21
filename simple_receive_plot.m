clc
clear

% Set IP of computer running the script (run ipconfig in CMD), also needs
% to be set in env.h and flashed onto device
server = tcpserver("192.168.0.101",4500);

disp("Waiting for client to connect.......");
fopen(server);
write(server,"Connected to server","string");
clc;
disp("Connected to client");

format long g

average_kbps = 0;
average_hz = 0;
avg_n = 0;

data_size = 4;  % size of the data - uint32_t = 4 bytes
block_size = 8; % how many uint32_t's sent per packet
inner_loop_length = 500; % how much data to collect between reporting frequency and speed
outer_loop_length = 1;   % how many times to run the inner loop before updating plot

x = outer_loop_length * inner_loop_length;


loop_data = zeros(x, block_size);

buffer_size_tracker = zeros(x);

filter_tracker = 0;
filter_time = 100;

file_name = "recording_"+string(datetime('now'),'yyyy-MM-dd_HH_mm_ss')+".xls";

clf

while 1
    for i = 1:outer_loop_length
        tic;
        for j = 1:inner_loop_length
            loop_data((i-1) * inner_loop_length + j, :) = read(server, block_size, "int32");
        end

        time = toc;
    
        frequency = (1 / time) * inner_loop_length;
    
        bps = frequency * data_size * block_size;
    
        kbps =  bps / 1024;
    
        average_kbps = ((average_kbps * avg_n) + kbps) / (avg_n + 1);
        average_hz = ((average_hz * avg_n) + frequency) / (avg_n + 1);
        avg_n = avg_n + 1;
    
        fprintf('%.2f kbps', kbps)
        fprintf(', avg = %.2f kbps', average_kbps)
        fprintf(', %.2f Hz', frequency)
        fprintf(', avg = %.2f Hz\r', average_hz)
    
    end
    
    
    for i = 1:block_size
        subplot(2, 4, i)
        plot(loop_data(:,i))
    end

    writematrix(loop_data, file_name,'WriteMode','append')
end


