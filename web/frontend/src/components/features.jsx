import React from "react";

export const Features = (props) => {
  return (
    <div id="features" className="text-center py-28">
      <div className="container mx-auto px-4">
        {/* Judul */}
        <div className="max-w-3xl mx-auto mb-16 mt-12 section-title">
          <h2 className="font-bold text-2xl">Features</h2>
        </div>

        {/* Grid responsif, selalu center */}
        <div className="grid grid-cols-2 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-5 gap-6 justify-items-center pb-12">
          {props.data
            ? props.data.map((d, i) => (
                <div
                  key={`${d.title}-${i}`}
                  className="bg-white p-10 rounded-lg shadow align-middle text-center w-full max-w-[220px]"
                >
                  <i className={`${d.icon}`}></i>
                  <h3 className="font-bold upp text-2xl py-3">{d.title}</h3>
                  <p className="text-gray-600 text-xl">{d.text}</p>
                </div>
              ))
            : "Loading..."}
        </div>
      </div>
    </div>
  );
};
