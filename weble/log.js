// 创建日志容器与清除按钮
const logDiv = document.createElement("div");
logDiv.id = "fixed-log-window";
document.body.appendChild(logDiv);

// 添加清除按钮
const clearBtn = document.createElement("button");
clearBtn.textContent = "清空日志";
clearBtn.className = "clear-log-btn";
logDiv.appendChild(clearBtn);

// 按钮点击事件
clearBtn.onclick = () => {
  logDiv.querySelectorAll(".log-entry").forEach(entry => entry.remove());
};

// 覆盖 console.log（保持原有功能）
const originalLog = console.log;
console.log = function(...args) {
  originalLog(...args);
  const logLine = document.createElement("div");
  logLine.className = "log-entry";
  logLine.textContent = args.join(" ");
  logDiv.appendChild(logLine);
  logDiv.scrollTop = logDiv.scrollHeight; // 自动滚动
};

// 样式增强
const style = document.createElement("style");
style.textContent = `
#fixed-log-window {
  /* 原有样式保持不变 */
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  height: 200px;
  background: rgba(0, 0, 0, 0.85);
  color: #fff;
  overflow-y: auto;
  padding: 40px 10px 10px; /* 顶部留出按钮空间 */
  font-family: monospace;
}
.clear-log-btn {
  position: fixed;
  right: 20px;
  bottom: 215px;
  padding: 4px 12px;
  background: #ff4444;
  border: none;
  color: white;
  cursor: pointer;
  border-radius: 3px;
}
.clear-log-btn:hover {
  background: #cc0000;
}
.log-entry {
  padding: 4px 0;
  border-bottom: 1px solid rgba(255,255,255,0.1);
}
`;
document.head.appendChild(style);

