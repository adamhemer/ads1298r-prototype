clc
clear

% Set IP of computer running the script (run ipconfig in CMD), also needs
% to be set in env.h and flashed onto device
server = tcpserver("192.168.0.101",4500);

availableDevices = mididevinfo;
device_name = availableDevices.output(2); % Currently set to windows wavetable synth, set to external midi device if necessary
device_name.Name
device = mididevice(device_name.ID);


OnMsg = midimsg('NoteOn',10,45,64);
midisend(device, OnMsg);

pause(0.5);

OnMsg = midimsg('NoteOn',10,46,64);
midisend(device, OnMsg);

pause(0.5);

for note = 0:127
    msg = midimsg('NoteOff',2,note,0);
    midisend(device, msg)
end

disp("Waiting for client to connect.......");
fopen(server);
write(server,"Connected to server","string");
clc;
disp("Connected to client");

format long g

average_kbps = 0;
average_hz = 0;
avg_n = 0;

data_size = 4; % uint32_t
block_size = 8; % how many values sent per packet
inner_loop_length = 500; % weird behaviour; changing to 512 causes massive drop in freq and data is lost

x = inner_loop_length;

filter_tracker = 0;
filter_time = 100;

% generate x axis for filtered and held value
n = inner_loop_length / filter_time;
repeats = 2;
fx = repmat(0:1:n,repeats,1);
fx = fx(:)';
fx = fx(2:2*n + 1) .* filter_time;

loop_data = zeros(x, block_size);
filter_data = zeros(x/filter_time, 1);

buffer_size_tracker = zeros(x);


filter_index = 1;

moving_average = 150000; % starting value for moving average
moving_factor = 0.9; % how much of the old to keep each time


file_name = "recording_"+string(datetime('now'),'yyyy-MM-dd_HH_mm_ss')+".xls";

clf

all_plot = true;

% can add new scales as desired
major = [1 3 5 6 8 10 12];
minor = [1 3 4 6 8 9 11];

% create a full scale from segment
scale = major; % swap this to any other scale
notes = 12;
inc = 12;
while major_scale(end) < 127
    scale = [scale major + inc];
    inc = inc + notes;
end

scale

start = false;
while ~start
    voltageData = read(server, 4, "uint8");
    voltageInt = typecast(uint8(flip(voltageData)), 'uint32');
    voltage = double(voltageInt) / 1000000;

    batData = read(server, 4, "uint8");
    batMins = typecast(uint8(flip(batData)), 'uint32');

    startData = read(server, 4, "uint8");
    if startData == [ 0x53, 0x54, 0x52, 0x54 ]
        start = true;
    end

    pause(1);
end

fprintf('Battery Voltage: %.2f V\r', voltage)
fprintf('Time Remaining: %d mins\r', batMins)

fprintf('System starting...\r')

maxruns = 10;

while 1
    tic;
    filter_tracker = 0;
    filter_index = 1;
    for j = 1:inner_loop_length
        loop_data((i-1) * inner_loop_length + j, :) = read(server, block_size, "int32");

        filter_tracker = filter_tracker + 1;
        if filter_tracker >= filter_time
            filter_signal = max(loop_data(j-filter_time+1:j, 2));
            filter_tracker = filter_tracker - filter_time;
            
            moving_average = moving_average * moving_factor + filter_signal * (1 - moving_factor);

            filter_signal = filter_signal - moving_average;
            filter_data(filter_index) = filter_signal;
            filter_index = filter_index + 1;
            note = (filter_signal / (2e5)) * 64;
            note = interp1(scale, scale, note + 64 ,'nearest','extrap');
            note = min(max(note, 0), 127)
            
            
            OnMsg = midimsg('NoteOn',2,note,64);
            midisend(device, OnMsg);
        end

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


    
    clf
    if all_plot
        for i = 1:block_size
            subplot(2, 4, i)
            hold on
            plot(loop_data(:,i))
            
            if i == 2
                filter_data;
                filter_data = repmat(filter_data, 2, 1);
                filter_data = filter_data(:);

                plot(fx, filter_data)

                plot([0 500], [moving_average moving_average])

                filter_data = [];
            end

            hold off
        end
    end

%      writematrix(loop_data, file_name,'WriteMode','append')

end

