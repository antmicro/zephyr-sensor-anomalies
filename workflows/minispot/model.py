import torch
import torch.nn as nn


class Net(nn.Module):
    def __init__(self, sensor_count, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sensor_count = sensor_count
        self.seq = nn.Sequential(
            nn.Conv1d(6, 16, kernel_size=5),
            nn.ReLU(),
            nn.Dropout(0.1),
            ResBlock(16, 16),
            nn.ReLU(),
            nn.MaxPool1d(2),
            nn.Dropout(0.1),
            ResBlock(16, 32),
            nn.ReLU(),
            nn.MaxPool1d(2),
            nn.Conv1d(32, 2, kernel_size=1),
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
        )

    def forward(self, x):
        x = torch.transpose(x, 1, 2)
        x = self.seq(x)
        return x


class ResBlock(nn.Module):
    def __init__(self, in_channel, out_channel):
        super().__init__()

        self.convb = nn.Conv1d(
            in_channels=in_channel, out_channels=out_channel, kernel_size=1
        )

        self.seq = nn.Sequential(
            nn.Conv1d(
                in_channels=in_channel,
                out_channels=out_channel,
                kernel_size=3,
                padding=1,
            ),
            nn.BatchNorm1d(out_channel),
            nn.ReLU(),
            nn.Conv1d(
                in_channels=out_channel,
                out_channels=out_channel,
                kernel_size=3,
                padding=1,
            ),
            nn.BatchNorm1d(out_channel),
        )

    def forward(self, x):
        return self.seq(x) + self.convb(x)


n = Net(6)

print(sum(p.numel() for p in n.parameters()))

print(n(torch.rand(5, 8, 6)).shape)


def get_model():
    return Net(6)
