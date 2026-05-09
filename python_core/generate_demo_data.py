"""
generate_demo_data.py — NerouRuntime 内置演示训练数据生成器

生成一个小型但有效的 EEG-BCI 模拟数据集，用于系统开箱即用体验。
输出：data/npz/demo_bci_4class.npz
格式：与 train.py 兼容的标准 NPZ（data, labels 键）

范式：4 类运动想象（Motor Imagery）
  - Class 0: 左手想象
  - Class 1: 右手想象
  - Class 2: 双脚想象
  - Class 3: 舌头想象

参数：8 通道 × 256 采样点（1 秒 @256Hz），200 样本
"""
import numpy as np
from pathlib import Path

def generate_demo_dataset(
    n_samples: int = 200,
    n_channels: int = 8,
    n_timepoints: int = 256,
    n_classes: int = 4,
    sample_rate: float = 256.0,
    seed: int = 42
):
    """生成模拟 EEG 运动想象数据。

    每个类别使用不同的频率特征模式以产生可区分的信号：
      - Class 0 (左手): 10Hz alpha 主导 + C3/C4 左侧激活
      - Class 1 (右手): 10Hz alpha 主导 + C3/C4 右侧激活
      - Class 2 (双脚): 20Hz beta 主导 + 中央通道激活
      - Class 3 (舌头): 30Hz gamma 主导 + 前额通道激活
    """
    rng = np.random.default_rng(seed)
    t = np.arange(n_timepoints) / sample_rate

    data = np.zeros((n_samples, n_channels, n_timepoints), dtype=np.float32)
    labels = np.zeros(n_samples, dtype=np.int64)

    samples_per_class = n_samples // n_classes

    for cls in range(n_classes):
        start = cls * samples_per_class
        end = start + samples_per_class
        labels[start:end] = cls

        for i in range(start, end):
            # 基础噪声（模拟 EEG 背景活动）
            noise = rng.normal(0, 5.0, (n_channels, n_timepoints)).astype(np.float32)

            # 1/f 粉红噪声特征
            freqs = np.fft.rfftfreq(n_timepoints, d=1.0/sample_rate)
            freqs[0] = 1.0  # 避免除零
            pink_spectrum = 1.0 / np.sqrt(freqs)
            for ch in range(n_channels):
                fft_noise = np.fft.rfft(noise[ch])
                noise[ch] = np.fft.irfft(fft_noise * pink_spectrum, n=n_timepoints)

            signal = noise.copy()

            if cls == 0:  # 左手 - alpha ERD in C4 (右脑)
                alpha = 15.0 * np.sin(2 * np.pi * 10 * t + rng.uniform(0, 2*np.pi))
                signal[5] += alpha   # C4 位置增强
                signal[6] += alpha * 0.7
                # C3 位置抑制
                signal[2] -= alpha * 0.5
                signal[3] -= alpha * 0.3

            elif cls == 1:  # 右手 - alpha ERD in C3 (左脑)
                alpha = 15.0 * np.sin(2 * np.pi * 10 * t + rng.uniform(0, 2*np.pi))
                signal[2] += alpha   # C3 位置增强
                signal[3] += alpha * 0.7
                # C4 位置抑制
                signal[5] -= alpha * 0.5
                signal[6] -= alpha * 0.3

            elif cls == 2:  # 双脚 - beta 主导 + 中央
                beta = 12.0 * np.sin(2 * np.pi * 20 * t + rng.uniform(0, 2*np.pi))
                signal[3] += beta    # Cz 附近
                signal[4] += beta * 0.8
                signal[2] += beta * 0.5
                signal[5] += beta * 0.5

            elif cls == 3:  # 舌头 - gamma + 前额
                gamma = 10.0 * np.sin(2 * np.pi * 30 * t + rng.uniform(0, 2*np.pi))
                signal[0] += gamma   # Fp1
                signal[1] += gamma * 0.9  # Fp2
                signal[2] += gamma * 0.4

            # 添加少量通道间相关性（模拟体积传导）
            for ch in range(1, n_channels):
                signal[ch] += 0.1 * signal[ch-1]

            # 微伏级标准化
            signal = signal / (np.std(signal) + 1e-8) * 20.0

            data[i] = signal

    # 打乱顺序
    perm = rng.permutation(n_samples)
    data = data[perm]
    labels = labels[perm]

    return data, labels


def main():
    output_dir = Path(__file__).resolve().parents[1] / "data" / "npz"
    output_dir.mkdir(parents=True, exist_ok=True)

    output_file = output_dir / "demo_bci_4class.npz"

    print("[generate_demo_data] Generating 4-class Motor Imagery demo dataset...")
    data, labels = generate_demo_dataset()

    np.savez_compressed(
        output_file,
        data=data,
        labels=labels
    )

    file_size = output_file.stat().st_size
    print(f"[generate_demo_data] Output: {output_file}")
    print(f"[generate_demo_data] Shape:  data={data.shape}, labels={labels.shape}")
    print(f"[generate_demo_data] Size:   {file_size / 1024:.1f} KB")
    print(f"[generate_demo_data] Classes: {np.unique(labels).tolist()}")
    print(f"[generate_demo_data] Samples per class: {[int(np.sum(labels == c)) for c in range(4)]}")
    print("[generate_demo_data] Done!")


if __name__ == "__main__":
    main()
