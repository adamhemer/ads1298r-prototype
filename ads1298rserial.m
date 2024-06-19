serial = serialport("COM7", 115200);

received = 0;

x = 1:1:2000;
y = zeros(2000, 4);

h = figure;

max = 2 ^ 24 - 1;
ylims = [-max, max];

updatePeriod = 0;
collected = 0;

while ishandle(h)
    if serial.NumBytesAvailable > 0
        line = serial.readline();
        % Split csv
        data = split(line, ", ");
        % Remove \n
        data(end) = [];
        % Check correct amount of values present
        if length(data) == 4
            received = received + 1;
            y = circshift(y, 1);
            y(1,:) = data';
        end

        % Redraw graph every 1000 updates ( 2 seconds at 500sps )
        updatePeriod = updatePeriod - 1;
        if updatePeriod < 0
            updatePeriod = 1000;
            plot(x, y(:,1))
            %ylim(ylims)
            drawnow
        end
    end
    
end



delete(serial)