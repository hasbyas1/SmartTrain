import React from "react";

export const Team = (props) => {
  return (
    <div id="team" className="text-center py-8">
      <div className="container mx-auto px-4">
        <div className="max-w-2xl mx-auto mb-8 section-title">
          <h2 className="font-bold text-2xl">Meet the Team</h2>
          <p className="text-gray-600">Great Persons Behind this Project</p>
        </div>

        {/* Flex center, wrap otomatis */}
        <div className="flex flex-wrap justify-center gap-14 md:gap-28">
          {props.data
            ? props.data.map((d, i) => (
                <div
                  key={`${d.name}-${i}`}
                  className="w-64 bg-white shadow-md rounded-lg overflow-hidden text-center"
                >
                  <img
                    src={d.img}
                    alt={d.name}
                    className="w-full h-64 object-cover"
                  />
                  <div className="p-4">
                    <h4 className="font-semibold">{d.name}</h4>
                    <p className="text-gray-500">{d.job}</p>
                  </div>
                </div>
              ))
            : "loading"}
        </div>
      </div>
    </div>
  );
};
