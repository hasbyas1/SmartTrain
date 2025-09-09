import React from "react";
import LiveCamPreview from "./LiveCamPreview";

export const About = (props) => {
  return (
    <div id="about" className="py-8">
      <div className="container mx-auto px-4">
        {/* Flex: urutan berubah di mobile */}
        <div className="flex flex-col-reverse md:flex-row md:items-none md:gap-8">
          {/* LiveCam */}
          <div className="w-full md:w-1/2">
            <LiveCamPreview
              wsUrl="http://192.168.1.202:5000/stream"
              autoPlay={true}
              controls={true}
            />
          </div>

          {/* Teks */}
          <div className="w-full md:w-1/2 mb-6 md:mb-0">
            <div className=" text-center md:text-left">
              <h2 className="font-bold text-2xl mb-4">Live Camera</h2>
              <p>{props.data ? props.data.paragraph : "loading..."}</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
