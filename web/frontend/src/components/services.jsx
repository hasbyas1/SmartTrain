import React from "react";

export const Services = (props) => {
  return (
    <div id="services" className="text-center py-8">
      <div className="container mx-auto px-6">
        <div className="section-title mb-8">
          <h2 className="font-bold text-2xl mb-2">Gallery</h2>
          <p className="text-gray-600">
            Lorem ipsum dolor sit amet, consectetur adipiscing elit duis sed
            dapibus leonec.
          </p>
        </div>

        {/* Container bento */}
        <div className="flex flex-col gap-6">
          {/* Row 1 */}
          <div className="flex flex-wrap justify-center gap-6">
            <div className="w-72 h-72 bg-white rounded-2xl"></div>
            <div className="w-96 h-72 bg-white rounded-2xl"></div>
            <div className="w-52 h-72 bg-white rounded-2xl"></div>
            <div className="flex-1 min-w-[150px] h-72 bg-white rounded-2xl"></div>
          </div>

          {/* Row 2 */}
          <div className="flex flex-wrap justify-center gap-6">
            <div className="w-80 h-72 bg-white rounded-2xl"></div>
            <div className="w-[590px] h-72 bg-white rounded-2xl max-w-full"></div>
            <div className="w-96 h-72 bg-white rounded-2xl"></div>
          </div>

          {/* Row 3 */}
          <div className="flex flex-wrap justify-center gap-6">
            <div className="w-96 h-72 bg-white rounded-2xl"></div>
            <div className="flex-1 min-w-[150px] h-72 bg-white rounded-2xl"></div>
            <div className="w-80 h-72 bg-white rounded-2xl"></div>
            <div className="flex-1 min-w-[150px] h-72 bg-white rounded-2xl"></div>
          </div>
        </div>
      </div>
    </div>
  );
};
