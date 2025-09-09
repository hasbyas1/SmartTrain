import React, { useState, useRef, useEffect } from "react";

export default function LiveCamPreview({
  url = `${process.env.REACT_APP_API_URL}/stream`,
  controls = true,
}) {
  const [connected, setConnected] = useState(true);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const containerRef = useRef(null);

  // update state saat user keluar fullscreen pakai ESC atau klik luar
  useEffect(() => {
    const handleChange = () => {
      setIsFullscreen(!!document.fullscreenElement);
    };
    document.addEventListener("fullscreenchange", handleChange);
    return () => document.removeEventListener("fullscreenchange", handleChange);
  }, []);

  const handleFullscreen = () => {
    const el = containerRef.current;
    if (!el) return;
    if (document.fullscreenElement) {
      document.exitFullscreen();
    } else {
      el.requestFullscreen?.();
    }
  };

  return (
    <div
      ref={containerRef}
      className="max-w-4xl mx-auto p-6 w-full"
    >
      <div className="bg-gray-900 rounded-2xl shadow-2xl border-4 border-gray-800 overflow-hidden">
        {/* Video area */}
        <div className="p-4">
          <div
            className="bg-black rounded-lg overflow-hidden relative"
            style={{ aspectRatio: "16/9" }}
          >
            <img
              src={url}
              alt="Camera Stream"
              className="w-full h-full object-contain"
              onError={() => setConnected(false)}
              onLoad={() => setConnected(true)}
            />
            {/* Live badge */}
            <div className="absolute top-3 left-3 flex items-center gap-2 bg-black/50 px-2 py-1 rounded-md">
              <div
                className={`w-2 h-2 rounded-full ${
                  connected ? "bg-red-500 animate-pulse" : "bg-gray-500"
                }`}
              />
              <span className="text-xs text-white">LIVE</span>
            </div>
            {/* Connection indicator */}
            <div className="absolute top-3 right-3 text-xs text-gray-300 bg-black/40 px-2 py-1 rounded-md">
              {connected ? "connected" : "disconnected"}
            </div>
          </div>
        </div>

        {/* Controls */}
        {controls && (
          <div className="bg-gray-800 px-6 py-4 flex items-center gap-4">
            <div className="flex-1 flex items-center justify-between text-xs text-gray-400">
              <div>{connected ? "Streaming" : "No feed"}</div>
              <div className="flex items-center gap-3">
                <div className="text-gray-400">Camera</div>
                <button
                  onClick={handleFullscreen}
                  className="p-1 rounded-md hover:bg-gray-700"
                >
                  {/* Toggle icon */}
                  {isFullscreen ? (
                    // Exit fullscreen icon
                    <svg
                      xmlns="http://www.w3.org/2000/svg"
                      className="h-5 w-5 text-gray-200"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth="2"
                      viewBox="0 0 24 24"
                    >
                      <path d="M9 3H5v4M3 3l6 6M15 21h4v-4M21 21l-6-6" />
                    </svg>
                  ) : (
                    // Enter fullscreen icon
                    <svg
                      xmlns="http://www.w3.org/2000/svg"
                      className="h-5 w-5 text-gray-200"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth="2"
                      viewBox="0 0 24 24"
                    >
                      <path d="M3 9V5h4M3 3l6 6M21 15v4h-4M21 21l-6-6" />
                    </svg>
                  )}
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
