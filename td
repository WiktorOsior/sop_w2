function t256 = zad23(input)
czworki = reshape(input,4,[]);
c = bitshift(czworki(1,:), 6) + bitshift(czworki(2,:), 4) + ...
    bitshift(czworki(3,:), 2) + czworki(4,:);
t256 = zeros(1,60);
for a = 1:60
    y = qammod(c,256);
    y_noise = awgn(y,a,"measured");
    z = qamdemod(y_noise,256);
    rec_row1 = bitshift(z, -6);           
    rec_row2 = bitand(bitshift(z, -4), 3);
    rec_row3 = bitand(bitshift(z, -2), 3);
    rec_row4 = bitand(z, 3); 
    x_rec_matrix = [rec_row1; rec_row2; rec_row3; rec_row4];
    x_rec = x_rec_matrix(:)';
    error = x_rec - input;
    t256(a) = nnz(error)/1000;
end

function t16 = zad22(input)
pary = reshape(input,2,[]);
x = bitshift(pary(1,:),2) + pary(2,:);
t16 = zeros(1,60);
for a = 1:60
    y = qammod(x,16);
    y_noise = awgn(y,a,"measured");
    z = qamdemod(y_noise,16);
    rec_row1 = bitshift(z, -2);
    rec_row2 = bitand(z, 3);
    x_rec_matrix = [rec_row1; rec_row2];
    x_rec = x_rec_matrix(:)';
    error = x_rec - input;
    t16(a) = nnz(error)/1000;
end

clc; clear; close all;
input = readmatrix("input.txt");
ser_21 = zad21(input);
ser_22 = zad22(input);
ser_23 = zad23(input);
figure;
snr = 1:60;
plot(snr, ser_21, 'DisplayName', '4-QAM');
hold on;
plot(snr, ser_22, 'DisplayName', '8-QAM');
hold on;
plot(snr, ser_23, 'DisplayName', '256-QAM');
hold on;
legend('show');
xlabel('SNR');
ylabel('SER');
grid on;
