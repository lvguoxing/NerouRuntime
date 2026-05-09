#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
model_presets.py — NerouRuntime 模型工厂
提供 EEGNet / ShallowConvNet / EEGConformer 实现及训练辅助函数。

模型说明：
  EEGNet      — 轻量级 CNN，< 10K 参数，适合边缘/嵌入式设备交付
  ShallowConvNet — 浅层 CNN，约 50K 参数，信号噪声大时鲁棒性好
  EEGConformer   — CNN + Temporal Attention 混合，约 500K 参数，精度优先场景

模型注册表（MODEL_REGISTRY）：
  通过 get_model(arch="eegconformer", ...) 按名称加载；
  train.py --model-template 参数映射到此。

网络输入格式: (B, num_channels, seq_len)  — 统一 Conv1d 流水线
默认支持 4 类运动想象分类（左手 / 右手 / 双脚 / 静息）

对齐规范：doc/核心脑电算法与数据工程规范.md
  - Max-Norm 约束（分类前权重范数 ≤ 1.0）
  - Focal Loss（α=0.6, γ=2.0）处理类不均衡
  - ReduceLROnPlateau（patience=10）+ CosineAnnealing 双调度器可选
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim


# ─── Max-Norm 权重约束 ─────────────────────────────────────────────────────────
# 规范要求：全连接判别前，强加约束权重向量范数不高于 max_norm
def apply_max_norm_constraint(model: nn.Module, max_norm: float = 1.0):
    """对模型中所有 Linear / Conv 层的权重施加 max-norm 约束。
    在每个 optimizer.step() 之后调用。"""
    with torch.no_grad():
        for name, param in model.named_parameters():
            if "weight" in name and param.dim() >= 2:
                # Per-output-unit max-norm: clamp the norm of each row
                norms = param.data.norm(2, dim=list(range(1, param.dim())), keepdim=True)
                desired = torch.clamp(norms, max=max_norm)
                param.data.mul_(desired / (norms + 1e-8))


# ─── Focal Loss ────────────────────────────────────────────────────────────────
# 规范要求：医学病理数据类不均衡，使用 Focal Loss（α=0.6, γ=2.0）
class FocalLoss(nn.Module):
    """Focal Loss: 对简单样本降权，聚焦难分样本。
    Lin et al. (2017) Focal Loss for Dense Object Detection.
    
    Args:
        alpha: 正类权重因子，默认 0.6（规范要求）
        gamma: 聚焦因子，默认 2.0（规范要求）
        label_smoothing: 标签平滑系数（与 CE 保持一致性）
    """
    def __init__(self, alpha: float = 0.6, gamma: float = 2.0,
                 label_smoothing: float = 0.05, num_classes: int = 4):
        super().__init__()
        self.alpha = alpha
        self.gamma = gamma
        self.label_smoothing = label_smoothing
        self.num_classes = num_classes

    def forward(self, logits: torch.Tensor, targets: torch.Tensor) -> torch.Tensor:
        # Apply label smoothing
        if self.label_smoothing > 0:
            with torch.no_grad():
                smooth_targets = torch.zeros_like(logits)
                smooth_targets.fill_(self.label_smoothing / (self.num_classes - 1))
                smooth_targets.scatter_(1, targets.unsqueeze(1), 1.0 - self.label_smoothing)
        
        log_probs = F.log_softmax(logits, dim=1)
        probs = torch.exp(log_probs)
        
        if self.label_smoothing > 0:
            # Soft focal loss with label smoothing
            focal_weight = (1 - probs) ** self.gamma
            loss = -self.alpha * focal_weight * smooth_targets * log_probs
            return loss.sum(dim=1).mean()
        else:
            # Standard focal loss
            nll = F.nll_loss(log_probs, targets, reduction='none')
            pt = probs.gather(1, targets.unsqueeze(1)).squeeze(1)
            focal_weight = self.alpha * (1 - pt) ** self.gamma
            return (focal_weight * nll).mean()


class EEGNet(nn.Module):
    """
    轻量级 EEG 分类网络 (Conv1d 版本)。
    适用于通道数 8~128、时序长度 64~1024 的脑电信号分类。
    
    对齐规范：
    - 时间卷积层探测器 (Temporal Filter)
    - 深度空域耦合 (Depthwise Spatial Conv, groups=channels)  
    - 点阵极简分离 (Separable Conv, 1x1)
    - Max-Norm 约束在训练循环中每步后调用
    """

    def __init__(self, num_channels: int = 64, seq_len: int = 256,
                 num_classes: int = 4, dropout: float = 0.5):
        super().__init__()

        F1 = 8      # 时间滤波器数量
        D  = 2      # 深度卷积倍增因子
        F2 = F1 * D # 深度卷积输出通道

        # ── 时间卷积 ─────────────────────────────────────────────────────────
        # 在时间维度上提取局部特征；kernel_size = sr//2 ≈ 128ms @ 256Hz
        t_kern = max(seq_len // 4, 3)
        self.temporal_conv = nn.Sequential(
            nn.Conv1d(num_channels, F1, kernel_size=t_kern, padding=t_kern // 2, bias=False),
            nn.BatchNorm1d(F1),
        )

        # ── 深度可分离卷积 ───────────────────────────────────────────────────
        self.depthwise = nn.Sequential(
            nn.Conv1d(F1, F2, kernel_size=16, groups=F1, padding=8, bias=False),
            nn.BatchNorm1d(F2),
            nn.ELU(),
            nn.AvgPool1d(kernel_size=4, stride=4),
            nn.Dropout(dropout),
        )

        # ── 逐点卷积 ─────────────────────────────────────────────────────────
        self.separable = nn.Sequential(
            nn.Conv1d(F2, F2, kernel_size=16, padding=8, bias=False),
            nn.Conv1d(F2, F2, kernel_size=1, bias=False),
            nn.BatchNorm1d(F2),
            nn.ELU(),
            nn.AvgPool1d(kernel_size=8, stride=8),
            nn.Dropout(dropout),
        )

        # ── 分类头 ───────────────────────────────────────────────────────────
        self.pool = nn.AdaptiveAvgPool1d(1)
        self.classifier = nn.Linear(F2, num_classes)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        x: (B, num_channels, seq_len)
        returns: (B, num_classes) logits
        """
        x = self.temporal_conv(x)   # → (B, F1, T)
        x = self.depthwise(x)       # → (B, F2, T/4)
        x = self.separable(x)       # → (B, F2, T/32)
        x = self.pool(x).squeeze(-1)  # → (B, F2)
        return self.classifier(x)   # → (B, num_classes)


class ShallowConvNet(nn.Module):
    """
    浅层卷积网络，约 50K 参数，适合采集噪声较大、数据量中等的场景。
    """
    def __init__(self, num_channels: int = 64, seq_len: int = 256, num_classes: int = 4):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv1d(num_channels, 40, kernel_size=25, padding=12, bias=False),
            nn.BatchNorm1d(40),
            nn.Conv1d(40, 40, kernel_size=1, bias=False),
            nn.BatchNorm1d(40),
            nn.ELU(),
            nn.AvgPool1d(kernel_size=75, stride=15),
            nn.Dropout(0.5),
        )
        self.pool = nn.AdaptiveAvgPool1d(1)
        self.classifier = nn.Linear(40, num_classes)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.features(x)
        x = self.pool(x).squeeze(-1)
        return self.classifier(x)


class EEGConformer(nn.Module):
    """
    EEG-Conformer：CNN 局部特征提取 + Transformer 时序依赖建模。

    参考：Song et al. (2022). EEG-Conformer: Convolutional Transformer for EEG Signal Decoding.
          IEEE Trans. Neural Syst. Rehabil. Eng.

    架构：
      - 时间卷积块（Temporal Conv）：Conv1d 提取局部时频特征
      - 深度空域卷积块（Spatial Conv）：Depthwise + Pointwise + AvgPool 降采样
      - Transformer Encoder：Multi-Head Self-Attention + FFN 建模全局时序依赖
      - 分类头（Dropout + Linear）

    适用场景：桌面 / 服务器端，数据量 ≥ 200 trial，精度要求高
    参数规模：约 500K（d_model=64, n_heads=4, n_layers=2）

    超参预设（via PRESET_EEGCONFORMER）：
      d_model=64, n_heads=4, n_layers=2, dropout=0.25
    """

    def __init__(self, num_channels: int = 22, seq_len: int = 256,
                 num_classes: int = 4, d_model: int = 64,
                 n_heads: int = 4, n_layers: int = 2, dropout: float = 0.25):
        super().__init__()

        # n_heads 必须整除 d_model
        while d_model % n_heads != 0 and n_heads > 1:
            n_heads -= 1

        # ── 时间卷积块 ────────────────────────────────────────────────────────
        t_kern = max(seq_len // 8, 3)
        self.temporal_conv = nn.Sequential(
            nn.Conv1d(num_channels, d_model, kernel_size=t_kern,
                      padding=t_kern // 2, bias=False),
            nn.BatchNorm1d(d_model),
            nn.ELU(),
        )

        # ── 深度空域卷积块 ────────────────────────────────────────────────────
        # groups = d_model 做 depthwise（真正 depthwise 需 groups == in_channels）
        self.spatial_conv = nn.Sequential(
            nn.Conv1d(d_model, d_model, kernel_size=16, groups=d_model,
                      padding=8, bias=False),
            nn.Conv1d(d_model, d_model, kernel_size=1, bias=False),
            nn.BatchNorm1d(d_model),
            nn.ELU(),
            nn.AvgPool1d(kernel_size=8, stride=8),
            nn.Dropout(dropout),
        )

        # ── Transformer Encoder ───────────────────────────────────────────────
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=d_model,
            nhead=n_heads,
            dim_feedforward=d_model * 4,
            dropout=dropout,
            batch_first=True,
            norm_first=True,   # Pre-LN：训练更稳定
        )
        self.transformer = nn.TransformerEncoder(encoder_layer, num_layers=n_layers)

        # ── 分类头 ────────────────────────────────────────────────────────────
        self.pool = nn.AdaptiveAvgPool1d(1)
        self.classifier = nn.Sequential(
            nn.Dropout(dropout),
            nn.Linear(d_model, num_classes),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """x: (B, num_channels, seq_len)  →  (B, num_classes)"""
        x = self.temporal_conv(x)      # (B, d_model, T)
        x = self.spatial_conv(x)       # (B, d_model, T/8)
        x = x.permute(0, 2, 1)        # (B, T/8, d_model)
        x = self.transformer(x)        # (B, T/8, d_model)
        x = x.permute(0, 2, 1)        # (B, d_model, T/8)
        x = self.pool(x).squeeze(-1)   # (B, d_model)
        return self.classifier(x)      # (B, num_classes)


# ─── 模型注册表 ────────────────────────────────────────────────────────────────

# 将 CLI --model-template 字符串映射到模型类
# 键名与 TrainingPage UI 下拉选项完全对应（小写）
MODEL_REGISTRY = {
    "eegnet":       EEGNet,
    "shallow":      ShallowConvNet,
    "shallowconvnet": ShallowConvNet,
    "eegconformer": EEGConformer,
    "conformer":    EEGConformer,
}

# 各模型预设超参（供 train.py 按模板加载）
PRESET_EEGNET = {
    "description": "轻量级 CNN，< 10K 参数，边缘/嵌入式设备首选",
    "min_trials": 50,
    "param_scale": "< 10K",
}

PRESET_SHALLOWCONVNET = {
    "description": "浅层 CNN，约 50K 参数，采集噪声大时鲁棒性好",
    "min_trials": 100,
    "param_scale": "~ 50K",
}

PRESET_EEGCONFORMER = {
    "description": "CNN + Temporal Attention，约 500K 参数，精度优先场景",
    "min_trials": 200,
    "param_scale": "~ 500K",
    "d_model": 64,
    "n_heads": 4,
    "n_layers": 2,
    "dropout": 0.25,
}


# ─── 工厂函数 ──────────────────────────────────────────────────────────────────

def get_model(num_channels: int = 64, seq_len: int = 256,
              num_classes: int = 4, arch: str = "eegnet",
              **kwargs) -> nn.Module:
    """
    返回指定架构的模型实例。

    arch: "eegnet" | "shallow" | "shallowconvnet" | "eegconformer" | "conformer"
    **kwargs: 额外超参（如 d_model / n_heads / n_layers / dropout，仅 EEGConformer 使用）
    """
    arch_key = arch.lower()
    if arch_key not in MODEL_REGISTRY:
        raise ValueError(
            f"未知架构: '{arch}'。可选: {list(MODEL_REGISTRY.keys())}"
        )

    model_cls = MODEL_REGISTRY[arch_key]

    if model_cls is EEGConformer:
        preset = {**PRESET_EEGCONFORMER, **kwargs}
        return EEGConformer(
            num_channels=num_channels,
            seq_len=seq_len,
            num_classes=num_classes,
            d_model=int(preset.get("d_model", 64)),
            n_heads=int(preset.get("n_heads", 4)),
            n_layers=int(preset.get("n_layers", 2)),
            dropout=float(preset.get("dropout", 0.25)),
        )

    return model_cls(num_channels=num_channels, seq_len=seq_len, num_classes=num_classes)


def get_criterion(num_classes: int = 4, use_focal: bool = True) -> nn.Module:
    """返回损失函数。
    
    use_focal=True (默认): FocalLoss(α=0.6, γ=2.0) — 按算法规范处理类不均衡
    use_focal=False:        CrossEntropyLoss + 标签平滑
    """
    if use_focal:
        return FocalLoss(alpha=0.6, gamma=2.0, label_smoothing=0.05, num_classes=num_classes)
    return nn.CrossEntropyLoss(label_smoothing=0.1)


def get_optimizer(model: nn.Module, lr: float = 5e-4) -> torch.optim.Optimizer:
    """AdamW + 权重衰减，适合 EEG 小数据集。"""
    return optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)


def get_scheduler(optimizer: torch.optim.Optimizer,
                  epochs: int = 30,
                  use_plateau: bool = True) -> torch.optim.lr_scheduler._LRScheduler:
    """返回学习率调度器。
    
    use_plateau=True (默认): ReduceLROnPlateau(patience=10) — 按算法规范
    use_plateau=False:        CosineAnnealingLR — 备选
    """
    if use_plateau:
        return optim.lr_scheduler.ReduceLROnPlateau(
            optimizer, mode='min', factor=0.5, patience=10, min_lr=1e-6, verbose=False
        )
    return optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs, eta_min=1e-6)


if __name__ == "__main__":
    import torch
    # 快速功能验证
    model = get_model(num_channels=64, seq_len=256, num_classes=4)
    dummy = torch.randn(4, 64, 256)   # batch=4, ch=64, time=256
    out   = model(dummy)
    print(f"输入: {dummy.shape}  →  输出: {out.shape}")
    print(f"参数量: {sum(p.numel() for p in model.parameters()):,}")
    assert out.shape == (4, 4), f"输出形状错误: {out.shape}"
    
    # 验证 FocalLoss
    criterion = get_criterion(num_classes=4, use_focal=True)
    labels = torch.tensor([0, 1, 2, 3])
    loss = criterion(out, labels)
    print(f"FocalLoss: {loss.item():.4f}")
    
    # 验证 max-norm
    apply_max_norm_constraint(model, max_norm=1.0)
    print("✓ Max-Norm 约束已应用")
    
    print("✓ 模型验证通过")
